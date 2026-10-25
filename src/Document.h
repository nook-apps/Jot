#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class MarkdownHighlighter;

// Один открытый файл. Текст живёт в QTextDocument редактора (TextEdit.textDocument):
// Document загружает и сохраняет его, вешает подсветку и следит за файлом на диске.
// Undo/redo — у самого TextEdit
class Document : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)
    // Пустой — новый, ещё не сохранённый документ
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    // Имя файла; у безымянного — первая строка текста, без разметки
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(bool untitled READ untitled NOTIFY pathChanged)
    // Безымянный и без текста — такую вкладку можно занять под открываемый файл
    Q_PROPERTY(bool blank READ blank NOTIFY statsChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY pathChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    // Сохраняется сам через паузу в наборе; только у документов с файлом
    Q_PROPERTY(bool autosave MEMBER m_autosave NOTIFY autosaveChanged)
    // Позиция курсора: от неё — проявленная разметка, строка и столбец
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY cursorChanged)
    Q_PROPERTY(int line READ line NOTIFY cursorChanged)
    Q_PROPERTY(int column READ column NOTIFY cursorChanged)
    Q_PROPERTY(int words READ words NOTIFY statsChanged)
    // Цвета подсветки — словарь из Theme.qml (см. MarkdownHighlighter::setPalette)
    Q_PROPERTY(QVariantMap palette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(int baseSize READ baseSize WRITE setBaseSize NOTIFY paletteChanged)
    Q_PROPERTY(QString codeFamily READ codeFamily WRITE setCodeFamily NOTIFY paletteChanged)

public:
    explicit Document(QObject *parent = nullptr);

    QQuickTextDocument *textDocument() const { return m_quickDoc; }
    void setTextDocument(QQuickTextDocument *doc);
    QString path() const { return m_path; }
    QString name() const { return m_name; }
    bool untitled() const { return m_path.isEmpty(); }
    bool blank() const;
    QString folder() const;
    bool modified() const;
    int cursorPosition() const { return m_cursor; }
    void setCursorPosition(int pos);
    int line() const;
    int column() const;
    int words() const { return m_words; }
    QVariantMap palette() const { return m_palette; }
    void setPalette(const QVariantMap &p);
    int baseSize() const { return m_baseSize; }
    void setBaseSize(int px);
    QString codeFamily() const { return m_codeFamily; }
    void setCodeFamily(const QString &f);

    // Нет файла — открываем пустой документ с этим путём: создастся при сохранении
    Q_INVOKABLE bool load(const QString &path);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QString &path);
    // Перечитать с диска, бросив правки
    Q_INVOKABLE bool reload();
    // Вернуть вкладку из сессии: файл (или безымянный, если path пуст) с несохранённым
    // черновиком поверх. Курсор — куда был
    Q_INVOKABLE bool restore(const QString &path, bool hasDraft, const QString &draft, int cursor);
    Q_INVOKABLE QString text() const;

    // Enter в пункте списка или цитате: продолжить разметку на новой строке,
    // пустой пункт — убрать маркер. Возвращает новую позицию курсора или -1,
    // если Enter обычный
    Q_INVOKABLE int continueBlock(int pos);
    // Обернуть выделение маркерами (**, *, `, ~~), повторно — снять.
    // Возвращает [начало, конец] нового выделения
    Q_INVOKABLE QVariantList toggleWrap(int start, int end, const QString &marker);

signals:
    void textDocumentChanged();
    void pathChanged();
    void nameChanged();
    void modifiedChanged();
    void autosaveChanged();
    void cursorChanged();
    void statsChanged();
    void paletteChanged();
    void saved();
    // Текст заменён целиком (открыт файл, перечитан с диска) — редактору
    // поставить курсор сюда: его собственный остался там, где кончилась вставка
    void loaded(int cursor);
    // Ошибка чтения или записи — для тоста
    void failed(const QString &message);
    // Файл изменили снаружи, а у нас несохранённые правки — спросить пользователя
    void changedOnDisk();

private:
    QTextDocument *doc() const;
    void setPath(const QString &path);
    // Заменить весь текст: межстрочный, сброс undo; modified — как велено
    void replaceText(const QString &text, bool modified);
    void updateName();
    bool writeTo(const QString &path);
    void fileChangedOnDisk();
    void countWords();

    QPointer<QQuickTextDocument> m_quickDoc;
    MarkdownHighlighter *m_highlighter = nullptr;
    QString m_path;
    QString m_name;
    bool m_crlf = false;
    bool m_autosave = true;
    int m_revision = -1;
    int m_cursor = 0;
    int m_words = 0;
    QVariantMap m_palette;
    int m_baseSize = 15;
    QString m_codeFamily;
    QFileSystemWatcher m_watcher;
    QTimer m_autosaveTimer;
    QTimer m_statsTimer;
    QTimer m_diskTimer;
};
