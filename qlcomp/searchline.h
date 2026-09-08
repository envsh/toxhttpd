#ifndef SEARCHLINE_H
#define SEARCHLINE_H

#include "compat34.h"

#ifdef QT3_BUILD
#include <qlistbox.h>          // QListBox
#include <qevent.h>            // QKeyEvent/QFocusEvent/QMouseEvent
#include <qpalette.h>          // QPalette
#include <qcursor.h>           // QCursor
typedef QListBox SearchList;
#else
#include <QListWidget>         // QListWidget
#include <QPalette>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
typedef QListWidget SearchList;
#endif

class SearchInputEdit : public QLineEdit {
public:
    SearchInputEdit(QWidget* parent = 0);

    void setPlaceholders(const QString& blurText, const QString& focusText);
    void showBlurPlaceholder();
    void releasePlaceholder();
    void capturePalette();
    bool isPlaceholderVisible() const { return m_showing; }

    QString actualText() const { return text(); }

protected:
    void focusInEvent(QFocusEvent* event);
    void focusOutEvent(QFocusEvent* event);
    void keyPressEvent(QKeyEvent* event);

private:
    QString m_blurText;
    QString m_focusText;
    bool m_showing;
    QPalette m_normalPalette;

    void updatePlaceholder(const QString& text);
    void setGrayed(bool gray);
};

class SearchLineEdit : public QFrame {
    Q_OBJECT
public:
    SearchLineEdit(QWidget* parent = 0);

    void setPlaceholders(const QString& blurText, const QString& focusText);
    void setSuggestions(const QStringList& items);
    QString text() const { return m_edit->actualText(); }
    void setText(const QString& text);
    void resetToPlaceholder();
    SearchInputEdit* lineEdit() const { return m_edit; }

signals:
    void textChanged(const QString& text);
    void cleared();
    void triggered(const QString& text);

protected:
    void paintEvent(QPaintEvent* event);
    bool eventFilter(QObject* watched, QEvent* event);

private slots:
    void onEditTextChanged(const QString& text);
    void onClearClicked();
    void onSuggestionClicked();

private:
    SearchInputEdit* m_edit;
    QLabel* m_iconLabel;
    QLabel* m_clearLabel;
    QStringList m_suggestions;
    QFrame* m_popup;
    SearchList* m_list;

    void showPopup();
    void hidePopup();
    void moveListSelection(int delta);
    void activateCurrentSuggestion();
    QString currentSuggestion() const;
};

#endif // SEARCHLINE_H