#include "Document.h"
#include "MarkdownHighlighter.h"

#include <QDir>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

namespace {
// Межстрочный интервал, % от высоты строки: текст дышит, но не рассыпается
constexpr int kLineHeight = 150;

QString expand(const QString &path)
{
    QString p = path;
    if (p.startsWith(QLatin1String("file://")))
        p = QUrl(p).toLocalFile();
    if (p == QLatin1String("~") || p.startsWith(QLatin1String("~/")))
        p = QDir::homePath() + p.mid(1);
    return p.isEmpty() ? p : QFileInfo(p).absoluteFilePath();
}

bool readText(const QString &path, QString *text, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return false;
    }
    *text = QString::fromUtf8(f.readAll());
    return true;
}
}

Document::Document(QObject *parent)
    : QObject(parent)
{
    // Сохраняем, когда набор утих: не на каждую букву, но и не теряем больше пары секунд
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(1500);
    connect(&m_autosaveTimer, &QTimer::timeout, this, [this] {
        if (m_autosave && modified() && !m_path.isEmpty())
            save();
    });
    m_statsTimer.setSingleShot(true);
    m_statsTimer.setInterval(300);
    connect(&m_statsTimer, &QTimer::timeout, this, &Document::countWords);
    // Редакторы и git пишут файл в несколько приёмов — смотрим, когда утихнет
    m_diskTimer.setSingleShot(true);
    m_diskTimer.setInterval(200);
    connect(&m_diskTimer, &QTimer::timeout, this, &Document::fileChangedOnDisk);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_diskTimer, qOverload<>(&QTimer::start));
}

QTextDocument *Document::doc() const
{
    return m_quickDoc ? m_quickDoc->textDocument() : nullptr;
}

void Document::setTextDocument(QQuickTextDocument *quickDoc)
{
    if (quickDoc == m_quickDoc)
        return;
    m_quickDoc = quickDoc;
    delete m_highlighter;
    m_highlighter = nullptr;
    if (QTextDocument *d = doc()) {
        m_highlighter = new MarkdownHighlighter(d);
        m_highlighter->setBaseSize(m_baseSize);
        m_highlighter->setCodeFamily(m_codeFamily);
        m_highlighter->setPalette(m_palette);
        connect(d, &QTextDocument::modificationChanged, this, &Document::modifiedChanged);
        connect(d, &QTextDocument::contentsChange, this, [this, d] {
            // Подсветка тоже шлёт contentsChange, но текст не трогает — ревизия та же
            if (d->revision() == m_revision)
                return;
            m_revision = d->revision();
            m_statsTimer.start();
            if (m_autosave && !m_path.isEmpty())
                m_autosaveTimer.start();
        });
    }
    emit textDocumentChanged();
    emit modifiedChanged();
}

void Document::updateName()
{
    QString name;
    if (!m_path.isEmpty()) {
        name = QFileInfo(m_path).fileName();
    } else if (QTextDocument *d = doc()) {
        // Первая непустая строка без знаков разметки — как у заметок в телефоне
        static const QRegularExpression markup(QStringLiteral(R"(^\s*(#{1,6}|>|[-*+]|\d+[.)])?\s*(\[[ xX]\]\s*)?)"));
        static const QRegularExpression emphasis(QStringLiteral("[*_`~=]"));
        for (QTextBlock b = d->begin(); b.isValid() && name.isEmpty(); b = b.next())
            name = b.text().remove(markup).remove(emphasis).trimmed();
        if (name.size() > 40)
            name = name.left(40).trimmed() + QStringLiteral("…");
    }
    if (name.isEmpty())
        name = QStringLiteral("Без названия");
    if (name != m_name) {
        m_name = name;
        emit nameChanged();
    }
}

bool Document::blank() const
{
    // По словам, а не по символам: пара пробелов — всё ещё пустая вкладка
    return m_path.isEmpty() && m_words == 0;
}

QString Document::text() const
{
    QTextDocument *d = doc();
    return d ? d->toPlainText() : QString();
}

QString Document::folder() const
{
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).absolutePath();
}

bool Document::modified() const
{
    QTextDocument *d = doc();
    return d && d->isModified();
}

void Document::setPath(const QString &path)
{
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    m_path = path;
    if (!m_path.isEmpty() && QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
    emit pathChanged();
    updateName();
}

bool Document::load(const QString &rawPath)
{
    QTextDocument *d = doc();
    if (!d)
        return false;
    const QString path = expand(rawPath);
    QString text;
    if (QFileInfo::exists(path)) {
        QString error;
        if (!readText(path, &text, &error)) {
            emit failed(QStringLiteral("Не открыть %1: %2").arg(QFileInfo(path).fileName(), error));
            return false;
        }
    }
    // Переводы строк Windows возвращаем как были при сохранении
    m_crlf = text.contains(QLatin1String("\r\n"));
    if (m_crlf)
        text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    replaceText(text, false);
    setPath(path);
    setCursorPosition(0);
    emit loaded(0);
    return true;
}

void Document::replaceText(const QString &text, bool modified)
{
    QTextDocument *d = doc();
    m_autosaveTimer.stop();
    d->setPlainText(text);
    // Межстрочный — свойство абзаца; новые строки наследуют его от соседней
    QTextCursor c(d);
    c.select(QTextCursor::Document);
    QTextBlockFormat bf;
    bf.setLineHeight(kLineHeight, QTextBlockFormat::ProportionalHeight);
    c.mergeBlockFormat(bf);
    d->clearUndoRedoStacks();
    d->setModified(modified);
    countWords();
}

bool Document::restore(const QString &path, bool hasDraft, const QString &draft, int cursor)
{
    QTextDocument *d = doc();
    if (!d)
        return false;
    // Файл пропал, пока Jot был закрыт, а черновика нет — вкладку не возвращаем
    if (!path.isEmpty() && !QFileInfo::exists(path) && !hasDraft)
        return false;
    if (!path.isEmpty() && QFileInfo::exists(path) && !load(path))
        return false;
    if (hasDraft) {
        replaceText(draft, true);
        setPath(path);
        // Черновик файла допишется на диск автосохранением, как если бы сбоя не было
        if (m_autosave && !m_path.isEmpty())
            m_autosaveTimer.start();
    }
    const int pos = qBound(0, cursor, d->characterCount() - 1);
    setCursorPosition(pos);
    emit loaded(pos);
    return true;
}

bool Document::reload()
{
    return m_path.isEmpty() ? false : load(m_path);
}

bool Document::writeTo(const QString &path)
{
    QTextDocument *d = doc();
    if (!d)
        return false;
    QString text = d->toPlainText();
    if (m_crlf)
        text.replace(QLatin1Char('\n'), QLatin1String("\r\n"));
    // Через временный файл и rename: оборванная запись не оставит полфайла
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(text.toUtf8()) < 0 || !f.commit()) {
        emit failed(QStringLiteral("Не сохранить %1: %2").arg(QFileInfo(path).fileName(), f.errorString()));
        return false;
    }
    d->setModified(false);
    emit saved();
    return true;
}

bool Document::save()
{
    m_autosaveTimer.stop();
    if (m_path.isEmpty())
        return false;
    if (!writeTo(m_path))
        return false;
    // Файла могло не быть до первой записи — теперь есть за чем следить
    if (!m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
    return true;
}

bool Document::saveAs(const QString &rawPath)
{
    m_autosaveTimer.stop();
    const QString path = expand(rawPath);
    if (path.isEmpty() || !writeTo(path))
        return false;
    setPath(path);
    return true;
}

void Document::fileChangedOnDisk()
{
    // Запись через rename выбивает файл из наблюдения — возвращаем
    if (QFileInfo::exists(m_path) && !m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
    QTextDocument *d = doc();
    if (!d || m_path.isEmpty() || !QFileInfo::exists(m_path))
        return;
    QString text, error;
    if (!readText(m_path, &text, &error))
        return;
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    // Свою же запись узнаём по содержимому
    if (text == d->toPlainText())
        return;
    if (!modified()) {
        const int cursor = m_cursor;
        load(m_path);
        const int pos = qMin(cursor, d->characterCount() - 1);
        setCursorPosition(pos);
        emit loaded(pos);
        return;
    }
    emit changedOnDisk();
}

void Document::setCursorPosition(int pos)
{
    QTextDocument *d = doc();
    m_cursor = pos;
    if (d && m_highlighter)
        m_highlighter->setActiveBlock(d->findBlock(pos).blockNumber());
    emit cursorChanged();
}

int Document::line() const
{
    QTextDocument *d = doc();
    return d ? d->findBlock(m_cursor).blockNumber() + 1 : 1;
}

int Document::column() const
{
    QTextDocument *d = doc();
    return d ? m_cursor - d->findBlock(m_cursor).position() + 1 : 1;
}

void Document::countWords()
{
    QTextDocument *d = doc();
    int n = 0;
    if (d) {
        const QString text = d->toPlainText();
        bool inWord = false;
        for (const QChar c : text) {
            const bool w = c.isLetterOrNumber();
            if (w && !inWord)
                ++n;
            inWord = w;
        }
    }
    if (n != m_words) {
        m_words = n;
        emit statsChanged();
    }
    if (m_path.isEmpty())
        updateName();
}

void Document::setPalette(const QVariantMap &p)
{
    m_palette = p;
    if (m_highlighter)
        m_highlighter->setPalette(p);
    emit paletteChanged();
}

void Document::setBaseSize(int px)
{
    m_baseSize = px;
    if (m_highlighter)
        m_highlighter->setBaseSize(px);
    emit paletteChanged();
}

void Document::setCodeFamily(const QString &f)
{
    m_codeFamily = f;
    if (m_highlighter)
        m_highlighter->setCodeFamily(f);
    emit paletteChanged();
}

int Document::continueBlock(int pos)
{
    QTextDocument *d = doc();
    if (!d)
        return -1;
    const QTextBlock block = d->findBlock(pos);
    const QString text = block.text();
    // Внутри блока кода Enter обычный
    if (block.previous().isValid() && block.previous().userState() > 0)
        return -1;
    static const QRegularExpression prefixRe(
        QStringLiteral(R"(^(\s*(?:>\s?)*)(?:(\s*)([-*+]|(\d{1,9})([.)]))(\s+)(\[[ xX]\]\s+)?)?)"));
    const QRegularExpressionMatch m = prefixRe.match(text);
    const int prefixEnd = m.capturedEnd(0);
    const bool isList = m.capturedLength(3) > 0;
    const bool isQuote = m.captured(1).contains(QLatin1Char('>'));
    if ((!isList && !isQuote) || pos - block.position() < prefixEnd)
        return -1;

    QTextCursor c(d);
    c.beginEditBlock();
    if (text.mid(prefixEnd).trimmed().isEmpty() && pos - block.position() == text.size()) {
        // Пустой пункт: второй Enter выходит из списка — маркер убираем
        c.setPosition(block.position());
        c.setPosition(block.position() + text.size(), QTextCursor::KeepAnchor);
        c.removeSelectedText();
        c.endEditBlock();
        return block.position();
    }
    QString next = m.captured(1);
    if (isList) {
        next += m.captured(2);
        if (m.capturedLength(4) > 0)
            next += QString::number(m.captured(4).toLongLong() + 1) + m.captured(5);
        else
            next += m.captured(3);
        next += m.captured(6);
        if (m.capturedLength(7) > 0)
            next += QStringLiteral("[ ] ");
    }
    c.setPosition(pos);
    c.insertText(QLatin1Char('\n') + next);
    c.endEditBlock();
    return pos + 1 + next.size();
}

QVariantList Document::toggleWrap(int start, int end, const QString &marker)
{
    QTextDocument *d = doc();
    if (!d || marker.isEmpty())
        return {start, end};
    if (start > end)
        std::swap(start, end);
    const int n = marker.size();
    auto textAt = [d](int s, int len) {
        QTextCursor c(d);
        c.setPosition(qBound(0, s, d->characterCount() - 1));
        c.setPosition(qBound(0, s + len, d->characterCount() - 1), QTextCursor::KeepAnchor);
        return c.selectedText();
    };
    QTextCursor c(d);
    c.beginEditBlock();
    QVariantList result;
    if (start >= n && textAt(start - n, n) == marker && textAt(end, n) == marker) {
        // Маркеры вокруг выделения — снимаем
        c.setPosition(end);
        c.setPosition(end + n, QTextCursor::KeepAnchor);
        c.removeSelectedText();
        c.setPosition(start - n);
        c.setPosition(start, QTextCursor::KeepAnchor);
        c.removeSelectedText();
        result = {start - n, end - n};
    } else {
        c.setPosition(end);
        c.insertText(marker);
        c.setPosition(start);
        c.insertText(marker);
        result = {start + n, end + n};
    }
    c.endEditBlock();
    return result;
}
