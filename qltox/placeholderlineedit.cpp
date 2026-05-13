#include "placeholderlineedit.h"

PlaceholderLineEdit::PlaceholderLineEdit(const QString& placeholder, QWidget* parent)
    : QLineEdit(parent), m_placeholder(placeholder), m_showingPlaceholder(true) {
    showPlaceholder();
}

void PlaceholderLineEdit::setPlaceholderText(const QString& text) {
    m_placeholder = text;
    if (m_showingPlaceholder) {
        showPlaceholder();
    }
}

void PlaceholderLineEdit::focusInEvent(QFocusEvent* event) {
    clearIfPlaceholder();
    QLineEdit::focusInEvent(event);
}

void PlaceholderLineEdit::focusOutEvent(QFocusEvent* event) {
    if (text().isEmpty()) {
        showPlaceholder();
    }
    QLineEdit::focusOutEvent(event);
}

void PlaceholderLineEdit::showPlaceholder() {
    setText(m_placeholder);
    m_showingPlaceholder = true;
}

void PlaceholderLineEdit::clearIfPlaceholder() {
    if (m_showingPlaceholder) {
        clear();
        m_showingPlaceholder = false;
    }
}
