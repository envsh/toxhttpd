#ifndef MDEDITOR_H
#define MDEDITOR_H

#include "compat34.h"
#include "mdhighlighter.h"
#include "mdpreview.h"
#include "mdtoolbar.h"

#ifdef QT3_BUILD
#include <qwidget.h>
#include <qtextedit.h>
#include <qsplitter.h>
#else
#include <QWidget>
#include <QTextEdit>
#include <QSplitter>
#endif

class MdEditor : public QWidget {
    Q_OBJECT
public:
    MdEditor(QWidget* parent = 0);
    void setMarkdown(const QString& markdown);
    QString toMarkdown() const;
    void loadSampleContent();

signals:
    void closed();
    void saveRequested();

protected:
    void closeEvent(QCloseEvent* e);
    void keyPressEvent(QKeyEvent* e);
    bool eventFilter(QObject* obj, QEvent* e);

private slots:
    void onUndo();
    void onRedo();
    void onInsertHeading(int level);
    void onInsertBold();
    void onInsertItalic();
    void onInsertUnderline();
    void onInsertStrikethrough();
    void onInsertInlineCode();
    void onInsertCodeBlock();
    void onInsertQuote();
    void onInsertUl();
    void onInsertOl();
    void onInsertTodo();
    void onInsertLink();
    void onInsertImage();
    void onInsertTable();
    void onInsertHr();
    void onInsertDate();
    void onInsertFootnote();
    void onInsertToc();
    void onTogglePreview();
    void onTextChanged();
    void onAutoSaveIntervalChanged(int minutes);
    void onAutoSaveTimeout();
    void onEditorScrollChanged();

private:
    QTextEdit* m_editor;
    MdHighlighter* m_highlighter;
    MdPreview* m_preview;
    MdToolbar* m_toolbar;
    QSplitter* m_splitter;
    bool m_previewVisible;
    bool m_modified;
    QTimer* m_autoSaveTimer;

    void wrapSelection(const QString& prefix, const QString& suffix);
    void insertAtCursor(const QString& text);
    void insertBlockPrefix(const QString& prefix);
    bool promptIfModified();
};

#endif
