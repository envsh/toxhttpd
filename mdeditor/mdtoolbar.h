#ifndef MDTOOLBAR_H
#define MDTOOLBAR_H

#include "compat34.h"
#include "emojiwidgets.h"

#ifdef QT3_BUILD
#include <qwidget.h>
#include <qpopupmenu.h>
#include <qpushbt.h>
#include <qmap.h>
#include <qcombobox.h>
#else
#include <QWidget>
#include <QMenu>
#include <QPushButton>
#include <QMap>
#include <QComboBox>
#endif

class QTextEdit;

class MdToolbar : public QWidget {
    Q_OBJECT
public:
    MdToolbar(QWidget* parent = 0);
    void setEditor(QTextEdit* editor);

signals:
    void undoRequested();
    void redoRequested();
    void insertHeading(int level);
    void insertBold();
    void insertItalic();
    void insertUnderline();
    void insertStrikethrough();
    void insertInlineCode();
    void insertCodeBlock();
    void insertQuote();
    void insertUl();
    void insertOl();
    void insertTodo();
    void insertLink();
    void insertImage();
    void insertTable();
    void insertHr();
    void insertDate();
    void insertFootnote();
    void insertToc();
    void togglePreview();
    void autoSaveIntervalChanged(int minutes);

private slots:
    void onHeadingClicked();
    void onDateClicked();
    void onButtonClicked();
    void onAutoSaveComboChanged(int index);

private:
    EmojiPushButton* makeBtn(const QString& id, const QString& display, const QString& tip);
    QTextEdit* m_editor;
    QMap<QPushButton*, QString> m_btnIds;
    QComboBox* m_autoSaveCombo;
#ifdef QT3_BUILD
    QPopupMenu* m_headingMenu;
#else
    QMenu* m_headingMenu;
#endif
};

#endif
