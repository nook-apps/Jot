#pragma once

#include <QColor>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVariantMap>

// «Живой исходник»: markdown остаётся текстом, но выглядит как документ —
// заголовки крупнее, жирное жирным, код моноширинным, а знаки разметки
// приглушены. На строке с курсором разметка проявляется: видно, что правишь.
//
// Разбор построчный, как у любого QSyntaxHighlighter; между строками помним
// только одно — внутри ли мы блока кода (``` или ~~~)
class MarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownHighlighter(QTextDocument *doc);

    // Цвета из Theme.qml: text, subtext, marker, markerActive, accent, link, code, codeBg
    void setPalette(const QVariantMap &palette);
    // Размер основного текста, px: от него считаются заголовки
    void setBaseSize(int px);
    void setCodeFamily(const QString &family);
    // Строка с курсором — на ней разметка не приглушается
    void setActiveBlock(int block);

protected:
    void highlightBlock(const QString &text) override;

private:
    enum State { Normal = 0, FenceBacktick = 1, FenceTilde = 2 };

    void highlightInline(const QString &text, int from, int to, bool active);
    QTextCharFormat markerFormat(bool active) const;
    QTextCharFormat codeFormat() const;
    void rebuild();

    QColor m_text, m_subtext, m_marker, m_markerActive, m_accent, m_link, m_code, m_codeBg;
    int m_baseSize = 15;
    QString m_codeFamily = QStringLiteral("monospace");
    int m_activeBlock = -1;
};
