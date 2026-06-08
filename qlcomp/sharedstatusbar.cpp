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
    , m_repositioning(false)
#ifdef QT3_BUILD
    , m_debounceTimer(nullptr)
    , m_pendingHide(false)
#endif
{
#ifdef QT3_BUILD
    m_bar = new QStatusBar(this, "innerbar");
    m_debounceTimer = new QTimer(this, "debounce");
    connect(m_debounceTimer, SIGNAL(timeout()),
            this, SLOT(onDebounceTimeout()));
#else
    m_bar = new QStatusBar(this);
    m_bar->setObjectName("innerbar");
#endif
    m_bar->setSizeGripEnabled(true);

#ifdef QT3_BUILD
    QBoxLayout *lay = new QBoxLayout(this, QBoxLayout::TopToBottom, 0, 0);
#else
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
#endif
    lay->addWidget(m_bar);

    qApp->installEventFilter(this);
}

SharedStatusBar::~SharedStatusBar()
{
    qApp->removeEventFilter(this);
#ifdef QT3_BUILD
    delete m_debounceTimer;
#endif
}

SharedStatusBar *SharedStatusBar::instance()
{
    if (!s_instance) {
        s_instance = new SharedStatusBar();
        QWidget *aw = qApp->activeWindow();
        if (aw) {
            s_instance->m_activeWindow = aw;
            s_instance->reposition();
        }
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
    if (m_repositioning) return false;

    if (event->type() == QEvent::WindowActivate) {
        // 同应用内窗口切换：停止 Qt3 debounce 定时器，直接跟随
#ifdef QT3_BUILD
        if (m_pendingHide) {
            m_pendingHide = false;
            m_debounceTimer->stop();
        }
#endif
        if (!watched->isWidgetType()) return false;
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (tw && tw != this) {
            m_activeWindow = tw;
            reposition();
        }
        return false;
    }

#ifdef QT3_BUILD
    // Qt3 无 ApplicationDeactivate 事件 → 用 debounce timer 模拟
    if (event->type() == QEvent::WindowDeactivate) {
        if (!watched->isWidgetType()) return false;
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (tw == m_activeWindow && !m_pendingHide) {
            m_pendingHide = true;
            m_debounceTimer->start(100, true); // single shot
        }
        return false;
    }
#else
    // Qt4 有 ApplicationDeactivate 事件 → 切到外部应用时隐藏
    if (event->type() == QEvent::ApplicationDeactivate) {
        hide();
        return false;
    }
#endif

    if (event->type() == QEvent::Close && watched == m_activeWindow) {
        m_activeWindow = nullptr;
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

#ifdef QT3_BUILD
void SharedStatusBar::onDebounceTimeout()
{
    // 100ms 内没有新的 WindowActivate → 真切换到外部应用 → 隐藏
    m_pendingHide = false;
    hide();
}
#endif

bool SharedStatusBar::isInGripArea(const QPoint &localPos) const
{
    return localPos.x() >= width() - GRIP_SIZE
        && localPos.y() >= height() - GRIP_SIZE;
}

void SharedStatusBar::handleGripPress(const QPoint &globalPos)
{
    if (!isVisible() || !m_activeWindow) { return; }
    m_dragging = true;
    m_dragStartGlobal = globalPos;
    m_windowStartGeo = m_activeWindow->geometry();
}

void SharedStatusBar::handleGripDrag(const QPoint &globalPos)
{
    if (!m_dragging || !m_activeWindow) { return; }
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
    if (m_repositioning || !m_activeWindow) return;
    m_repositioning = true;

    if (!m_activeWindow->isVisible()) {
        m_repositioning = false;
        return;
    }
    QPoint bl = m_activeWindow->mapToGlobal(
        QPoint(0, m_activeWindow->height()));
    move(bl.x(), bl.y());
    resize(m_activeWindow->width(), m_bar->sizeHint().height());
    if (!isVisible()) show();

    m_repositioning = false;
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
