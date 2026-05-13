#ifndef PLACEHOLDERLINEEDIT_H
#define PLACEHOLDERLINEEDIT_H

#include "compat34.h"

class PlaceholderLineEdit : public QLineEdit {
    Q_OBJECT
public:
    PlaceholderLineEdit(const QString& placeholder, QWidget* parent = 0);
    
    void setPlaceholderText(const QString& text);
    QString placeholderText() const { return m_placeholder; }
    
protected:
    void focusInEvent(QFocusEvent* event);
    void focusOutEvent(QFocusEvent* event);
    
private:
    QString m_placeholder;
    bool m_showingPlaceholder;
    
    void showPlaceholder();
    void clearIfPlaceholder();
};

#endif
