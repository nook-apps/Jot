#include "MarkdownHighlighter.h"

#include <QHash>
#include <QRegularExpression>
#include <QTextDocument>

namespace {
// Масштаб заголовков от основного текста; с четвёртого — только жирность
constexpr double kHeadingScale[] = {1.6, 1.35, 1.18, 1.0, 1.0, 1.0};

QColor withAlpha(QColor c, double a)
{
    c.setAlphaF(a);
    return c;
}

const QRegularExpression &re(const char *pattern)
{
    // Выражения живут столько же, сколько программа: компилируются один раз
    static QHash<const char *, QRegularExpression> cache;
    auto it = cache.find(pattern);
    if (it == cache.end())
        it = cache.insert(pattern, QRegularExpression(QString::fromUtf8(pattern)));
    return *it;
}
}

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
}

void MarkdownHighlighter::setPalette(const QVariantMap &p)
{
    auto col = [&p](const char *key, const QColor &fallback) {
        const QVariant v = p.value(QLatin1String(key));
        return v.isValid() ? v.value<QColor>() : fallback;
    };
    m_text = col("text", Qt::white);
    m_subtext = col("subtext", Qt::lightGray);
    m_marker = col("marker", Qt::gray);
    m_markerActive = col("markerActive", Qt::lightGray);
    m_accent = col("accent", QColor(0xb4, 0x8c, 0xff));
    m_link = col("link", m_accent);
    m_code = col("code", m_text);
    m_codeBg = col("codeBg", Qt::transparent);
    rebuild();
}

void MarkdownHighlighter::setBaseSize(int px)
{
    if (px == m_baseSize || px <= 0)
        return;
    m_baseSize = px;
    rebuild();
}

void MarkdownHighlighter::setCodeFamily(const QString &family)
{
    if (family == m_codeFamily || family.isEmpty())
        return;
    m_codeFamily = family;
    rebuild();
}

void MarkdownHighlighter::setActiveBlock(int block)
{
    if (block == m_activeBlock)
        return;
    const int old = m_activeBlock;
    m_activeBlock = block;
    if (!document())
        return;
    // Перекрашиваем только две строки — прежнюю и новую
    if (old >= 0) {
        const QTextBlock b = document()->findBlockByNumber(old);
        if (b.isValid())
            rehighlightBlock(b);
    }
    const QTextBlock b = document()->findBlockByNumber(block);
    if (b.isValid())
        rehighlightBlock(b);
}

void MarkdownHighlighter::rebuild()
{
    if (document())
        rehighlight();
}

QTextCharFormat MarkdownHighlighter::markerFormat(bool active) const
{
    QTextCharFormat f;
    f.setForeground(active ? m_markerActive : m_marker);
    return f;
}

QTextCharFormat MarkdownHighlighter::codeFormat() const
{
    QTextCharFormat f;
    f.setFontFamilies({m_codeFamily});
    f.setForeground(m_code);
    f.setBackground(m_codeBg);
    return f;
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    const bool active = currentBlock().blockNumber() == m_activeBlock;
    const int prev = previousBlockState() < 0 ? Normal : previousBlockState();
    setCurrentBlockState(Normal);

    // Поверх уже наложенного: жирное внутри заголовка остаётся крупным
    auto paint = [this](int start, int len, const QTextCharFormat &f) {
        for (int i = start; i < start + len; ++i) {
            QTextCharFormat cur = format(i);
            cur.merge(f);
            setFormat(i, 1, cur);
        }
    };

    // ── Блоки кода ──────────────────────────────────────────────────
    const QRegularExpressionMatch fence = re(R"(^\s{0,3}(`{3,}|~{3,})(.*)$)").match(text);
    if (prev != Normal) {
        const QChar want = prev == FenceBacktick ? QLatin1Char('`') : QLatin1Char('~');
        if (fence.hasMatch() && fence.captured(1).at(0) == want && fence.captured(2).trimmed().isEmpty()) {
            paint(0, text.size(), codeFormat());
            paint(fence.capturedStart(1), fence.capturedLength(1), markerFormat(active));
            return; // блок закрыт, состояние — Normal
        }
        setCurrentBlockState(prev);
        paint(0, text.size(), codeFormat());
        return;
    }
    if (fence.hasMatch()) {
        setCurrentBlockState(fence.captured(1).at(0) == QLatin1Char('`') ? FenceBacktick : FenceTilde);
        paint(0, text.size(), codeFormat());
        paint(fence.capturedStart(1), fence.capturedLength(1), markerFormat(active));
        // Язык блока — фирменным цветом
        QTextCharFormat lang;
        lang.setForeground(m_accent);
        paint(fence.capturedStart(2), fence.capturedLength(2), lang);
        return;
    }

    QTextCharFormat accent;
    accent.setForeground(active ? m_accent : withAlpha(m_accent, 0.6));

    // ── Горизонтальная линия (раньше списков: «- - -» — линия, а не пункт) ──
    if (re(R"(^\s{0,3}([-*_])(\s*\1){2,}\s*$)").match(text).hasMatch()) {
        paint(0, text.size(), markerFormat(active));
        return;
    }

    int bodyStart = 0;

    // ── Заголовки ───────────────────────────────────────────────────
    const QRegularExpressionMatch h = re(R"(^(\s{0,3})(#{1,6})(\s+|$)(.*?)(\s+#+\s*)?$)").match(text);
    if (h.hasMatch()) {
        const int level = h.capturedLength(2);
        QTextCharFormat hf;
        hf.setFontWeight(QFont::Bold);
        hf.setForeground(m_text);
        hf.setProperty(QTextFormat::FontPixelSize, qRound(m_baseSize * kHeadingScale[level - 1]));
        paint(0, text.size(), hf);
        paint(h.capturedStart(2), h.capturedLength(2), accent);
        if (h.capturedLength(5) > 0)
            paint(h.capturedStart(5), h.capturedLength(5), markerFormat(active));
        highlightInline(text, h.capturedStart(4), h.capturedEnd(4), active);
        return;
    }

    // ── Таблицы ─────────────────────────────────────────────────────
    if (re(R"(^\s*\|.*\|\s*$)").match(text).hasMatch()) {
        if (re(R"(^\s*\|(\s*:?-+:?\s*\|)+\s*$)").match(text).hasMatch()) {
            paint(0, text.size(), markerFormat(active));
            return;
        }
        highlightInline(text, 0, text.size(), active);
        for (int i = 0; i < text.size(); ++i)
            if (text.at(i) == QLatin1Char('|') && (i == 0 || text.at(i - 1) != QLatin1Char('\\')))
                paint(i, 1, markerFormat(active));
        return;
    }

    // ── Цитаты ──────────────────────────────────────────────────────
    const QRegularExpressionMatch q = re(R"(^(\s{0,3}>\s?)+)").match(text);
    if (q.hasMatch()) {
        QTextCharFormat qf;
        qf.setForeground(m_subtext);
        qf.setFontItalic(true);
        paint(0, text.size(), qf);
        for (int i = 0; i < q.capturedEnd(0); ++i)
            if (text.at(i) == QLatin1Char('>'))
                paint(i, 1, accent);
        bodyStart = q.capturedEnd(0);
    }

    // ── Списки и задачи ─────────────────────────────────────────────
    const QRegularExpressionMatch li =
        re(R"(^(\s*)([-*+]|\d{1,9}[.)])(\s+)(\[[ xX]\](\s+|$))?)").match(text, bodyStart, QRegularExpression::NormalMatch,
                                                                    QRegularExpression::AnchorAtOffsetMatchOption);
    if (li.hasMatch()) {
        paint(li.capturedStart(2), li.capturedLength(2), accent);
        if (li.capturedLength(4) > 0) {
            paint(li.capturedStart(4), 3, markerFormat(active));
            const bool done = text.at(li.capturedStart(4) + 1) != QLatin1Char(' ');
            if (done) {
                // Галочка — акцентом, выполненный пункт зачёркнут и приглушён
                paint(li.capturedStart(4) + 1, 1, accent);
                QTextCharFormat df;
                df.setForeground(m_subtext);
                df.setFontStrikeOut(true);
                paint(li.capturedEnd(0), text.size() - li.capturedEnd(0), df);
            }
        }
        bodyStart = li.capturedEnd(0);
    }

    // Комментарий HTML на всю строку — служебное
    if (re(R"(^\s*<!--.*-->\s*$)").match(text).hasMatch()) {
        paint(0, text.size(), markerFormat(active));
        return;
    }

    highlightInline(text, bodyStart, text.size(), active);
}

void MarkdownHighlighter::highlightInline(const QString &text, int from, int to, bool active)
{
    if (from >= to)
        return;
    // Занятое кодом и адресами ссылок — внутри них разметки нет
    QList<bool> taken(text.size(), false);
    auto isFree = [&taken](int s, int e) {
        for (int i = s; i < e; ++i)
            if (taken[i])
                return false;
        return true;
    };
    auto take = [&taken](int s, int e) {
        for (int i = s; i < e; ++i)
            taken[i] = true;
    };
    auto paint = [this](int start, int len, const QTextCharFormat &f) {
        for (int i = start; i < start + len; ++i) {
            QTextCharFormat cur = format(i);
            cur.merge(f);
            setFormat(i, 1, cur);
        }
    };
    const QTextCharFormat marker = markerFormat(active);
    const QStringView view = QStringView(text).first(to);

    // Код в строке: `…`, ``…``
    for (auto it = re(R"((`+)(.+?)\1)").globalMatchView(view, from); it.hasNext();) {
        const auto m = it.next();
        QTextCharFormat cf = codeFormat();
        paint(m.capturedStart(2), m.capturedLength(2), cf);
        paint(m.capturedStart(1), m.capturedLength(1), marker);
        paint(m.capturedStart(0) + m.capturedLength(0) - m.capturedLength(1), m.capturedLength(1), marker);
        take(m.capturedStart(0), m.capturedEnd(0));
    }

    // Ссылки и картинки: [текст](адрес), ![подпись](адрес)
    for (auto it = re(R"((!?\[)([^\]]*)(\]\()([^)\s]*(?:\s+"[^"]*")?)(\)))").globalMatchView(view, from); it.hasNext();) {
        const auto m = it.next();
        if (!isFree(m.capturedStart(0), m.capturedEnd(0)))
            continue;
        QTextCharFormat lf;
        lf.setForeground(m_link);
        lf.setFontUnderline(true);
        paint(m.capturedStart(2), m.capturedLength(2), lf);
        for (int g : {1, 3, 4, 5})
            paint(m.capturedStart(g), m.capturedLength(g), marker);
        take(m.capturedStart(3), m.capturedEnd(5));
    }

    // Адреса как есть: <https://…> и https://…
    for (auto it = re(R"(<?\bhttps?://[^\s<>()]+>?)").globalMatchView(view, from); it.hasNext();) {
        const auto m = it.next();
        if (!isFree(m.capturedStart(0), m.capturedEnd(0)))
            continue;
        QTextCharFormat lf;
        lf.setForeground(m_link);
        paint(m.capturedStart(0), m.capturedLength(0), lf);
        take(m.capturedStart(0), m.capturedEnd(0));
    }

    struct Span {
        const char *pattern;
        int markerLen;
        QTextCharFormat format;
    };
    QTextCharFormat bold, italic, strike, mark;
    bold.setFontWeight(QFont::Bold);
    italic.setFontItalic(true);
    strike.setFontStrikeOut(true);
    strike.setForeground(m_subtext);
    mark.setBackground(withAlpha(m_accent, 0.22));
    const Span spans[] = {
        {R"((\*\*|__)(?=\S)(.+?)(?<=\S)\1)", 2, bold},
        // Одиночная звёздочка не должна съедать половину «**»
        {R"((?<![*\w])\*(?=[^\s*])(.+?)(?<=[^\s*])\*(?!\*))", 1, italic},
        {R"((?<![_\w])_(?=[^\s_])(.+?)(?<=[^\s_])_(?![_\w]))", 1, italic},
        {R"(~~(?=\S)(.+?)(?<=\S)~~)", 2, strike},
        {R"(==(?=\S)(.+?)(?<=\S)==)", 2, mark},
    };
    for (const Span &s : spans) {
        for (auto it = re(s.pattern).globalMatchView(view, from); it.hasNext();) {
            const auto m = it.next();
            if (!isFree(m.capturedStart(0), m.capturedEnd(0)))
                continue;
            paint(m.capturedStart(0), m.capturedLength(0), s.format);
            paint(m.capturedStart(0), s.markerLen, marker);
            paint(m.capturedEnd(0) - s.markerLen, s.markerLen, marker);
        }
    }
}
