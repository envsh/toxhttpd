#include "mdeditor.h"
#include <qlayout.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qdatetime.h>
#ifndef QT3_BUILD
#include <QTextCursor>
#endif

MdEditor::MdEditor(QWidget* parent)
    : QWidget(parent), m_previewVisible(true) {
    qSetWindowTitle(this, qFromUtf8("Markdown 编辑器"));
    resize(1080, 630);

    QVBoxLayout* mainLay = new QVBoxLayout(this);
    mainLay->setMargin(4);
    mainLay->setSpacing(4);

    m_toolbar = new MdToolbar(this);
    mainLay->addWidget(m_toolbar, 0);

#ifdef QT3_BUILD
    m_splitter = new QSplitter(QSplitter::Horizontal, this);
#else
    m_splitter = new QSplitter(Qt::Horizontal, this);
#endif
    mainLay->addWidget(m_splitter, 1);

    m_editor = new QTextEdit(m_splitter);
#ifdef QT3_BUILD
    m_editor->setTextFormat(Qt::PlainText);
#else
    m_editor->setAcceptRichText(false);
#endif
    m_preview = new MdPreview(m_splitter);

    m_highlighter = new MdHighlighter(m_editor);
    m_toolbar->setEditor(m_editor);

    m_splitter->addWidget(m_editor);
    m_splitter->addWidget(m_preview);

    connect(m_toolbar, SIGNAL(undoRequested()), this, SLOT(onUndo()));
    connect(m_toolbar, SIGNAL(redoRequested()), this, SLOT(onRedo()));
    connect(m_toolbar, SIGNAL(insertHeading(int)), this, SLOT(onInsertHeading(int)));
    connect(m_toolbar, SIGNAL(insertBold()), this, SLOT(onInsertBold()));
    connect(m_toolbar, SIGNAL(insertItalic()), this, SLOT(onInsertItalic()));
    connect(m_toolbar, SIGNAL(insertUnderline()), this, SLOT(onInsertUnderline()));
    connect(m_toolbar, SIGNAL(insertStrikethrough()), this, SLOT(onInsertStrikethrough()));
    connect(m_toolbar, SIGNAL(insertInlineCode()), this, SLOT(onInsertInlineCode()));
    connect(m_toolbar, SIGNAL(insertCodeBlock()), this, SLOT(onInsertCodeBlock()));
    connect(m_toolbar, SIGNAL(insertQuote()), this, SLOT(onInsertQuote()));
    connect(m_toolbar, SIGNAL(insertUl()), this, SLOT(onInsertUl()));
    connect(m_toolbar, SIGNAL(insertOl()), this, SLOT(onInsertOl()));
    connect(m_toolbar, SIGNAL(insertTodo()), this, SLOT(onInsertTodo()));
    connect(m_toolbar, SIGNAL(insertLink()), this, SLOT(onInsertLink()));
    connect(m_toolbar, SIGNAL(insertImage()), this, SLOT(onInsertImage()));
    connect(m_toolbar, SIGNAL(insertTable()), this, SLOT(onInsertTable()));
    connect(m_toolbar, SIGNAL(insertHr()), this, SLOT(onInsertHr()));
    connect(m_toolbar, SIGNAL(insertDate()), this, SLOT(onInsertDate()));
    connect(m_toolbar, SIGNAL(insertFootnote()), this, SLOT(onInsertFootnote()));
    connect(m_toolbar, SIGNAL(insertToc()), this, SLOT(onInsertToc()));
    connect(m_toolbar, SIGNAL(togglePreview()), this, SLOT(onTogglePreview()));
    connect(m_editor, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
}

static QString mdRepeatChar(int count, char ch) {
    QString s;
    for (int i = 0; i < count; ++i) {
        s += ch;
    }
    return s;
}

void MdEditor::setMarkdown(const QString& markdown) {
#ifdef QT3_BUILD
    m_editor->setText(markdown);
#else
    m_editor->setPlainText(markdown);
#endif
    m_preview->setMarkdown(markdown);
}

QString MdEditor::toMarkdown() const {
#ifdef QT3_BUILD
    return m_editor->text();
#else
    return m_editor->toPlainText();
#endif
}

void MdEditor::loadSampleContent() {
    QString sample = qFromUtf8(
        "# Markdown 编辑器示例\n\n"
        "这是一段 **粗体** 文本，这是 *斜体*，这是 ~~删除线~~。\n\n"
        "## 功能列表\n\n"
        "- 实时预览\n"
        "- 语法高亮\n"
        "- 工具栏操作\n"
        "- 任务列表：\n"
        "  - [x] 粗体\n"
        "  - [x] 斜体\n"
        "  - [ ] 删除线\n\n"
        "## 代码示例\n\n"
        "```cpp\n"
        "int main() {\n"
        "    return 0;\n"
        "}\n"
        "```\n\n"
        "## 表格\n\n"
        "| 名称 | 版本 |\n"
        "|------|------|\n"
        "| Qt3  | 3.3.8 |\n"
        "| Qt4  | 4.8.7 |\n\n"
        "> 这是一段引用文本。\n\n"
        "---\n\n"
        "## 链接与图片\n\n"
        "[Qt 官网](https://www.qt.io)\n\n"
        "![图片](https://via.placeholder.com/150)\n\n"
        "## 脚注\n\n"
        "这是一段带有脚注的文本[^1]。\n\n"
        "[^1]: 这是脚注内容。\n\n"
        "示例结束。\n");
    setMarkdown(sample);
}

void MdEditor::closeEvent(QCloseEvent* e) {
    emit closed();
    QWidget::closeEvent(e);
}

void MdEditor::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        close();
        return;
    }
    QWidget::keyPressEvent(e);
}

#ifdef QT3_BUILD

void MdEditor::wrapSelection(const QString& prefix, const QString& suffix) {
    QString sel = m_editor->selectedText();
    if (!sel.isEmpty()) {
        m_editor->insert(prefix + sel + suffix);
    } else {
        m_editor->insert(prefix + "text" + suffix);
    }
}

void MdEditor::insertAtCursor(const QString& text) {
    m_editor->insert(text);
}

void MdEditor::insertBlockPrefix(const QString& prefix) {
    m_editor->insert(prefix);
}

#else

void MdEditor::wrapSelection(const QString& prefix, const QString& suffix) {
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection()) {
        QString sel = cursor.selectedText();
        cursor.insertText(prefix + sel + suffix);
    } else {
        cursor.insertText(prefix + "text" + suffix);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, suffix.length());
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 4);
        m_editor->setTextCursor(cursor);
    }
}

void MdEditor::insertAtCursor(const QString& text) {
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(text);
    m_editor->setTextCursor(cursor);
}

void MdEditor::insertBlockPrefix(const QString& prefix) {
    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.insertText(prefix);
    m_editor->setTextCursor(cursor);
}

#endif

void MdEditor::onUndo() {
    m_editor->undo();
}

void MdEditor::onRedo() {
    m_editor->redo();
}

void MdEditor::onInsertHeading(int level) {
    insertBlockPrefix(mdRepeatChar(level, '#') + " ");
}

void MdEditor::onInsertBold() {
    wrapSelection("**", "**");
}

void MdEditor::onInsertItalic() {
    wrapSelection("*", "*");
}

void MdEditor::onInsertUnderline() {
    wrapSelection("__", "__");
}

void MdEditor::onInsertStrikethrough() {
    wrapSelection("~~", "~~");
}

void MdEditor::onInsertInlineCode() {
    wrapSelection("`", "`");
}

void MdEditor::onInsertCodeBlock() {
    insertAtCursor("```\ncode\n```");
}

void MdEditor::onInsertQuote() {
    insertBlockPrefix("> ");
}

void MdEditor::onInsertUl() {
    insertBlockPrefix("- ");
}

void MdEditor::onInsertOl() {
    insertBlockPrefix("1. ");
}

void MdEditor::onInsertTodo() {
    insertBlockPrefix("- [ ] ");
}

void MdEditor::onInsertLink() {
    wrapSelection("[", "](url)");
}

void MdEditor::onInsertImage() {
    wrapSelection("![alt](", ")");
}

void MdEditor::onInsertTable() {
    insertAtCursor("| Header 1 | Header 2 | Header 3 |\n"
                   "|----------|----------|----------|\n"
                   "| Cell 1   | Cell 2   | Cell 3   |\n");
}

void MdEditor::onInsertHr() {
    insertAtCursor("\n---\n\n");
}

void MdEditor::onInsertDate() {
    QString date = QDate::currentDate().toString("yyyy-MM-dd");
    insertAtCursor(date);
}

void MdEditor::onInsertFootnote() {
    wrapSelection("[^", "]");
}

void MdEditor::onInsertToc() {
    insertAtCursor("[toc]\n\n");
}

void MdEditor::onTogglePreview() {
    m_previewVisible = !m_previewVisible;
    if (m_previewVisible) {
        m_preview->show();
    } else {
        m_preview->hide();
    }
}

void MdEditor::onTextChanged() {
#ifdef QT3_BUILD
    m_preview->setMarkdown(m_editor->text());
#else
    m_preview->setMarkdown(m_editor->toPlainText());
#endif
}
