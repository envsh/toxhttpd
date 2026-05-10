#include "emojiwidgets.h"
#include "emojiutil.h"

#ifdef EMOJI_RENDER_QT34

// ============ EmojiLabel ============

#ifdef QT3_BUILD

EmojiLabel::EmojiLabel(QWidget* parent, const char* name)
    : QLabel(parent, name) {}
EmojiLabel::EmojiLabel(const QString& text, QWidget* parent, const char* name)
    : QLabel(text, parent, name) {}

void EmojiLabel::drawContents(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QLabel::drawContents(p); return; }
    QRect cr = contentsRect();
    int m = margin();
    QRect tr(cr.x() + m, cr.y() + m, cr.width() - 2 * m, cr.height() - 2 * m);
    EmojiRenderer::instance().drawText(*p, tr, t);
}

#else // QT4

EmojiLabel::EmojiLabel(QWidget* parent, Qt::WindowFlags f) : QLabel(parent, f) {}
EmojiLabel::EmojiLabel(const QString& text, QWidget* parent, Qt::WindowFlags f)
    : QLabel(text, parent, f) {}

void EmojiLabel::paintEvent(QPaintEvent* event) {
    QLabel::paintEvent(event);
    QString t = text();
    if (textHasEmoji(t) && !pixmap()) {
        QPainter p(this);
        p.setClipRect(event->rect());
        QRect tr = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
        EmojiRenderer::instance().drawText(p, tr, t);
    }
}

#endif

// ============ EmojiPushButton ============

#ifdef QT3_BUILD

EmojiPushButton::EmojiPushButton(QWidget* parent, const char* name)
    : QPushButton(parent, name) {}
EmojiPushButton::EmojiPushButton(const QString& text, QWidget* parent, const char* name)
    : QPushButton(text, parent, name) {}

void EmojiPushButton::drawButtonLabel(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QPushButton::drawButtonLabel(p); return; }
    QRect r = rect();
    EmojiRenderer::instance().drawText(*p, r, t);
}

#else

EmojiPushButton::EmojiPushButton(QWidget* parent) : QPushButton(parent) {}
EmojiPushButton::EmojiPushButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {}

void EmojiPushButton::paintEvent(QPaintEvent* event) {
    if (!textHasEmoji(text())) { QPushButton::paintEvent(event); return; }
    QPainter p(this);
    QStyleOptionButton opt;
    opt.initFrom(this);
    opt.text = QString();
    style()->drawControl(QStyle::CE_PushButton, &opt, &p, this);
    QRect cr = style()->subElementRect(QStyle::SE_PushButtonContents, &opt, this);
    EmojiRenderer::instance().drawText(p, cr, text());
}

#endif

// ============ EmojiToolButton ============

#ifdef QT3_BUILD

EmojiToolButton::EmojiToolButton(QWidget* parent, const char* name)
    : QToolButton(parent, name) {}

void EmojiToolButton::drawButtonLabel(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QToolButton::drawButtonLabel(p); return; }
    EmojiRenderer::instance().drawText(*p, rect(), t);
}

#else

EmojiToolButton::EmojiToolButton(QWidget* parent) : QToolButton(parent) {}

void EmojiToolButton::paintEvent(QPaintEvent* event) {
    QToolButton::paintEvent(event);
    if (textHasEmoji(text())) {
        QPainter p(this);
        EmojiRenderer::instance().drawText(p, rect(), text());
    }
}

#endif

// ============ EmojiCheckBox ============

#ifdef QT3_BUILD

EmojiCheckBox::EmojiCheckBox(QWidget* parent, const char* name)
    : QCheckBox(parent, name) {}
EmojiCheckBox::EmojiCheckBox(const QString& text, QWidget* parent, const char* name)
    : QCheckBox(text, parent, name) {}

void EmojiCheckBox::drawButtonLabel(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QCheckBox::drawButtonLabel(p); return; }
    EmojiRenderer::instance().drawText(*p, rect(), t);
}

#else

EmojiCheckBox::EmojiCheckBox(QWidget* parent) : QCheckBox(parent) {}
EmojiCheckBox::EmojiCheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {}

void EmojiCheckBox::paintEvent(QPaintEvent* event) {
    if (!textHasEmoji(text())) { QCheckBox::paintEvent(event); return; }
    QPainter p(this);
    QStyleOptionButton opt;
    opt.initFrom(this);
    opt.text = QString();
    style()->drawControl(QStyle::CE_CheckBox, &opt, &p, this);
    QRect cr = style()->subElementRect(QStyle::SE_CheckBoxContents, &opt, this);
    EmojiRenderer::instance().drawText(p, cr, text());
}

#endif

// ============ EmojiGroupBox ============

#ifdef QT3_BUILD

EmojiGroupBox::EmojiGroupBox(QWidget* parent, const char* name)
    : QGroupBox(parent, name) {}
EmojiGroupBox::EmojiGroupBox(const QString& title, QWidget* parent, const char* name)
    : QGroupBox(title, parent, name) {}

void EmojiGroupBox::paintEvent(QPaintEvent* event) {
    QGroupBox::paintEvent(event);
    QString t = title();
    if (!textHasEmoji(t)) return;
    QPainter p(this);
    p.setClipRect(event->rect());
    QFontMetrics fm = p.fontMetrics();
    int indent = 10;
    int w = rect().width();
    QRect tr(indent, 0, w - 2 * indent, fm.height());
    EmojiRenderer::instance().drawText(p, tr, t);
}

#else

EmojiGroupBox::EmojiGroupBox(QWidget* parent) : QGroupBox(parent) {}
EmojiGroupBox::EmojiGroupBox(const QString& title, QWidget* parent)
    : QGroupBox(title, parent) {}

void EmojiGroupBox::paintEvent(QPaintEvent* event) {
    QGroupBox::paintEvent(event);
    if (textHasEmoji(title())) {
        QPainter p(this);
        QFontMetrics fm = p.fontMetrics();
        QRect tr(10, 0, fm.width(title()), fm.height());
        EmojiRenderer::instance().drawText(p, tr, title());
    }
}

#endif

// ============ EmojiLineEdit ============

#ifdef QT3_BUILD

EmojiLineEdit::EmojiLineEdit(QWidget* parent, const char* name)
    : QLineEdit(parent, name) {}

void EmojiLineEdit::drawContents(QPainter* p) {
    QString t = text();
    if (!textHasEmoji(t)) { QLineEdit::drawContents(p); return; }
    EmojiRenderer::instance().drawText(*p, rect(), t);
}

#else

EmojiLineEdit::EmojiLineEdit(QWidget* parent) : QLineEdit(parent) {}

void EmojiLineEdit::paintEvent(QPaintEvent* event) {
    QLineEdit::paintEvent(event);
    if (textHasEmoji(text())) {
        QPainter p(this);
        p.setClipRect(event->rect());
        EmojiRenderer::instance().drawText(p, rect(), text());
    }
}

#endif

#endif // EMOJI_RENDER_QT34
