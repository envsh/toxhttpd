#include "mdhighlighter.h"

#ifdef QT3_BUILD

MdHighlighter::MdHighlighter(QTextEdit* parent)
    : QSyntaxHighlighter(parent) {
    QFont boldFont;
    boldFont.setBold(true);
    m_rules.push_back({QRegExp("^#{1,6}\\s+.*"), boldFont, Qt::darkBlue, true});
    m_rules.push_back({QRegExp("\\*\\*[^*]+\\*\\*"), boldFont, QColor(), true});
    m_rules.push_back({QRegExp("__[^_]+__"), boldFont, QColor(), true});

    QFont italicFont;
    italicFont.setItalic(true);
    m_rules.push_back({QRegExp("\\*[^*]+\\*"), italicFont, QColor(), true});
    m_rules.push_back({QRegExp("_[^_]+_"), italicFont, QColor(), true});

    QFont codeFont;
    codeFont.setFamily("monospace");
    m_rules.push_back({QRegExp("`[^`]+`"), codeFont, QColor(), true});

    m_rules.push_back({QRegExp("\\[[^\\]]+\\]\\([^)]+\\)"), QFont(), Qt::darkMagenta, false});
    m_rules.push_back({QRegExp("!\\[[^\\]]*\\]\\([^)]+\\)"), QFont(), Qt::darkGreen, false});
    m_rules.push_back({QRegExp("^>.*"), QFont(), Qt::gray, false});
    m_rules.push_back({QRegExp("^(-{3,}|\\*{3,}|_{3,})$"), QFont(), Qt::gray, false});
    m_rules.push_back({QRegExp("^\\s*[-*+]\\s+"), QFont(), Qt::darkRed, false});
    m_rules.push_back({QRegExp("^\\s*\\d+\\.\\s+"), QFont(), Qt::darkRed, false});
    m_rules.push_back({QRegExp("^\\s*-\\s+\\[[ x]\\]\\s+"), QFont(), Qt::darkCyan, false});
    m_rules.push_back({QRegExp("#[\\w]+"), QFont(), Qt::darkYellow, false});
}

int MdHighlighter::highlightParagraph(const QString& text, int) {
    for (size_t i = 0; i < m_rules.size(); ++i) {
        const Rule& rule = m_rules[i];
        QRegExp expr(rule.pattern);
        int index = expr.search(text);
        while (index >= 0) {
            int length = expr.matchedLength();
            if (rule.useFont) {
                setFormat(index, length, rule.font, rule.color);
            } else if (rule.color.isValid()) {
                setFormat(index, length, rule.color);
            }
            index = expr.search(text, index + length);
        }
    }
    return 0;
}

#else

MdHighlighter::MdHighlighter(QTextEdit* parent)
    : QSyntaxHighlighter(parent) {
    QTextCharFormat headingFmt;
    headingFmt.setFontWeight(QFont::Bold);
    headingFmt.setForeground(Qt::darkBlue);
    m_rules.push_back({QRegExp("^#{1,6}\\s+.*"), headingFmt});

    QTextCharFormat boldFmt;
    boldFmt.setFontWeight(QFont::Bold);
    m_rules.push_back({QRegExp("\\*\\*[^*]+\\*\\*"), boldFmt});
    m_rules.push_back({QRegExp("__[^_]+__"), boldFmt});

    QTextCharFormat italicFmt;
    italicFmt.setFontItalic(true);
    m_rules.push_back({QRegExp("\\*[^*]+\\*"), italicFmt});
    m_rules.push_back({QRegExp("_[^_]+_"), italicFmt});

    QTextCharFormat codeFmt;
    codeFmt.setFontFamily("monospace");
    codeFmt.setBackground(QColor(240, 240, 240));
    m_rules.push_back({QRegExp("`[^`]+`"), codeFmt});

    QTextCharFormat linkFmt;
    linkFmt.setForeground(Qt::darkMagenta);
    m_rules.push_back({QRegExp("\\[[^\\]]+\\]\\([^)]+\\)"), linkFmt});

    QTextCharFormat imageFmt;
    imageFmt.setForeground(Qt::darkGreen);
    m_rules.push_back({QRegExp("!\\[[^\\]]*\\]\\([^)]+\\)"), imageFmt});

    QTextCharFormat commentFmt;
    commentFmt.setForeground(Qt::gray);
    commentFmt.setFontItalic(true);
    m_rules.push_back({QRegExp("^>.*"), commentFmt});

    QTextCharFormat hrFmt;
    hrFmt.setForeground(Qt::gray);
    m_rules.push_back({QRegExp("^(-{3,}|\\*{3,}|_{3,})$"), hrFmt});

    QTextCharFormat listFmt;
    listFmt.setForeground(Qt::darkRed);
    m_rules.push_back({QRegExp("^\\s*[-*+]\\s+"), listFmt});
    m_rules.push_back({QRegExp("^\\s*\\d+\\.\\s+"), listFmt});

    QTextCharFormat todoFmt;
    todoFmt.setForeground(Qt::darkCyan);
    m_rules.push_back({QRegExp("^\\s*-\\s+\\[[ x]\\]\\s+"), todoFmt});

    QTextCharFormat tagFmt;
    tagFmt.setForeground(Qt::darkYellow);
    m_rules.push_back({QRegExp("#[\\w]+"), tagFmt});
}

void MdHighlighter::highlightBlock(const QString& text) {
    for (size_t i = 0; i < m_rules.size(); ++i) {
        const Rule& rule = m_rules[i];
        QRegExp expr(rule.pattern);
        int index = expr.indexIn(text);
        while (index >= 0) {
            int length = expr.matchedLength();
            setFormat(index, length, rule.format);
            index = expr.indexIn(text, index + length);
        }
    }
}

#endif
