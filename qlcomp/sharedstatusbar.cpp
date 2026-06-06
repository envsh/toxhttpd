#include "sharedstatusbar.h"

SharedStatusBar *SharedStatusBar::s_instance = nullptr;

static const int GRIP_SIZE = 16;

SharedStatusBar::SharedStatusBar()
#ifdef QT3_BUILD
    : QWidget(nullptr, "sharedstatusbar",
              WStyle_Customize | WStyle_NoBorder | WStyle_StaysOnTop)
#else
    : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
#endif
    , m_bar(nullptr)
    , m_activeWindow(nullptr)
    , m_dragging(false)
{
#ifdef QT3_BUILD
    m_bar = new QStatusBar(this, "innerbar");
#else
    m_bar = new QStatusBar(this);
    m_bar->setObjectName("innerbar");
#endif
    m_bar->setSizeGripEnabled(true);

    qApp->installEventFilter(this);
}

SharedStatusBar::~SharedStatusBar() {}

SharedStatusBar *SharedStatusBar::instance()
{
    if (!s_instance) {
        s_instance = new SharedStatusBar();
        s_instance->show();
    }
    return s_instance;
}

void SharedStatusBar::showMessage(const QString &msg, int timeout)
{
#ifdef QT3_BUILD
    m_bar->message(msg, timeout);
#else
    m_bar->showMessage(msg, timeout);
#endif
}

void SharedStatusBar::clearMessage()
{
#ifdef QT3_BUILD
    m_bar->clear();
#else
    m_bar->clearMessage();
#endif
}

void SharedStatusBar::addWidget(QWidget *w, int stretch)
{
    m_bar->addWidget(w, stretch);
}

void SharedStatusBar::addPermanentWidget(QWidget *w, int stretch)
{
#ifdef QT3_BUILD
    m_bar->addWidget(w, stretch, true);
#else
    m_bar->addPermanentWidget(w, stretch);
#endif
}

void SharedStatusBar::removeWidget(QWidget *w)
{
    m_bar->removeWidget(w);
}

bool SharedStatusBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::WindowActivate) {
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (tw && tw != this) {
            m_activeWindow = tw;
            show();
            reposition();
        }
        return false;
    }
    if (event->type() == QEvent::WindowDeactivate) {
        if (!qApp->activeWindow())
            hide();
        return false;
    }
    if ((event->type() == QEvent::Move ||
         event->type() == QEvent::Resize) &&
        watched == m_activeWindow) {
        reposition();
        return false;
    }
    return false;
}

bool SharedStatusBar::isInGripArea(const QPoint &localPos) const
{
    return localPos.x() >= width() - GRIP_SIZE
        && localPos.y() >= height() - GRIP_SIZE;
}

void SharedStatusBar::handleGripPress(const QPoint &globalPos)
{
    if (!isVisible() || !m_activeWindow) return;
    m_dragging = true;
    m_dragStartGlobal = globalPos;
    m_windowStartGeo = m_activeWindow->geometry();
}

void SharedStatusBar::handleGripDrag(const QPoint &globalPos)
{
    if (!m_dragging || !m_activeWindow) return;
    int dx = globalPos.x() - m_dragStartGlobal.x();
    int dy = globalPos.y() - m_dragStartGlobal.y();
    QRect g = m_windowStartGeo;
    g.setWidth ((g.width() + dx) > m_activeWindow->minimumWidth()
                ? g.width() + dx
                : m_activeWindow->minimumWidth());
    g.setHeight((g.height() + dy) > m_activeWindow->minimumHeight()
                ? g.height() + dy
                : m_activeWindow->minimumHeight());
    m_activeWindow->setGeometry(g);
}

void SharedStatusBar::handleGripRelease()
{
    m_dragging = false;
}

void SharedStatusBar::reposition()
{
    if (!m_activeWindow || !m_activeWindow->isVisible()) {
        hide();
        return;
    }
    QPoint bl = m_activeWindow->mapToGlobal(
        QPoint(0, m_activeWindow->height()));
    move(bl.x(), bl.y());
    resize(m_activeWindow->width(), height());
    if (!isVisible()) show();
}

bool SharedStatusBar::event(QEvent *e)
{
    if (e->type() == QEvent::MouseButtonPress) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (me->button() == Qt::LeftButton && isInGripArea(me->pos())) {
            handleGripPress(me->globalPos());
            return true;
        }
    }
    if (e->type() == QEvent::MouseMove) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (m_dragging) {
            handleGripDrag(me->globalPos());
            return true;
        }
    }
    if (e->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        if (m_dragging && me->button() == Qt::LeftButton) {
            handleGripRelease();
            return true;
        }
    }
    return QWidget::event(e);
}
