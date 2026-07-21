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
#include <qvaluelist.h>
#include <vector>
typedef double qreal;
#else
#include <QWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QVector>
#endif

struct ScrollEntry {
    int editorLine;
    qreal editorY;
    qreal previewY;
};

class MdEditor : public QWidget {
    Q_OBJECT
public:
    MdEditor(QWidget* parent = 0);
    ~MdEditor();
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
    void onPreviewScrollChanged();

private:
    QTextEdit* m_editor;
    MdHighlighter* m_highlighter;
    MdPreview* m_preview;
    MdToolbar* m_toolbar;
    QSplitter* m_splitter;
    bool m_previewVisible;
    bool m_modified;
    QTimer* m_autoSaveTimer;
#ifdef QT3_BUILD
    std::vector<ScrollEntry> m_scrollMap;
#else
    QVector<ScrollEntry> m_scrollMap;
#endif
    bool m_syncing;
    QTimer* m_scrollSyncGuard;

    void wrapSelection(const QString& prefix, const QString& suffix);
    void insertAtCursor(const QString& text);
    void insertBlockPrefix(const QString& prefix);
    bool promptIfModified();
    void buildScrollMap();
    int findEntryByEditorY(qreal y) const;
    int findEntryByPreviewY(qreal y) const;
};

#endif
