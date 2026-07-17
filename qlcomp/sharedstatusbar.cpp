#include "sharedstatusbar.h"
#ifdef QT3_BUILD
#include <qpainter.h>
#include <qpen.h>
#include <qcursor.h>
#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#else
#include <QPainter>
#ifdef Q_OS_LINUX
#include <QX11Info>
#include <X11/Xlib.h>
#endif
#endif

SharedStatusBar *SharedStatusBar::s_instance = nullptr;

static const int GRIP_SIZE = 16;

SharedStatusBar::SharedStatusBar()
#ifdef QT3_BUILD
    : QWidget(nullptr, "sharedstatusbar",
              WStyle_Customize | WStyle_Tool | WStyle_NoBorder)
#else
    : QWidget(nullptr,
              Qt::Tool | Qt::FramelessWindowHint)
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
    m_bar->setSizeGripEnabled(false);
    setMouseTracking(true);

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

    // Show：widget 变为可见时触发（兜底，不受 Qt::Tool 限制）
    if (event->type() == QEvent::Show) {
        if (!watched->isWidgetType()) return false;
        QWidget *tw = static_cast<QWidget*>(watched)->topLevelWidget();
        if (!tw || tw == this) return false;
#ifdef QT3_BUILD
        if (tw->inherits("QLabel")) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#else
        if (tw->windowFlags() & (Qt::ToolTip | Qt::Popup)) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#endif
        if (tw->width() < 350) {
            return false;
        }
        if (minimumWidth() > tw->width()) {
            return false;
        }
        m_activeWindow = tw;
        reposition();
        return false;
    }

#ifndef QT3_BUILD
    // ApplicationActivate：应用被激活时触发（watched 是 qApp，兜底）
    if (event->type() == QEvent::ApplicationActivate) {
        QWidget *aw = qApp->activeWindow();
        if (!aw || aw == this) return false;
        if (aw->inherits("DesktopLyrics")) return false;
        if (aw->inherits("ScreenshotRegionSelector")) return false;
        if (aw->inherits("ScreenshotPreviewDialog")) return false;
        if (aw->width() < 350) return false;
        if (minimumWidth() > aw->width()) return false;
        m_activeWindow = aw;
        reposition();
        return false;
    }
#endif

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
        if (!tw || tw == this) return false;
#ifdef QT3_BUILD
        // TipLabel 是顶层 QLabel — 跳过，不跟踪
        if (tw->inherits("QLabel")) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#else
        // Qt4 tooltips/popups 不跟踪
        if (tw->windowFlags() & (Qt::ToolTip | Qt::Popup)) return false;
        if (tw->inherits("DesktopLyrics")) return false;
        if (tw->inherits("ScreenshotRegionSelector")) return false;
        if (tw->inherits("ScreenshotPreviewDialog")) return false;
#endif
		// 窗口宽或高小于 350px 时不跟随
		if (tw->width() < 350) {
			return false;
		}
        // 状态栏最小宽度大于活动窗口宽度时不跟随
        if (minimumWidth() > tw->width()) {
            return false;
        }		
        // 状态栏可见且新窗口不遮挡当前位置时，不跟随
        if (isVisible() && !geometry().intersects(tw->frameGeometry())) {
            // return false;
        }
        m_activeWindow = tw;
        reposition();
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

    if (event->type() == QEvent::Hide) {
        if (watched == m_activeWindow && watched->isWidgetType()) {
            QWidget *w = static_cast<QWidget*>(watched);
            if (w->isTopLevel()) {
                QTimer::singleShot(50, this, SLOT(retrack()));
                return false;
            }
        }
        return false;
    }

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
    // hide();
}
#endif

void SharedStatusBar::retrack()
{
    QWidget *aw = qApp->activeWindow();
    if (!aw || aw == this) {
        hide();
        m_activeWindow = nullptr;
        return;
    }
    if (aw != m_activeWindow) {
        if (aw->inherits("ScreenshotRegionSelector") || aw->inherits("ScreenshotPreviewDialog")) {
            return;
        }
		if (minimumWidth() > aw->width()) { return; }
        if (isVisible() && !geometry().intersects(aw->frameGeometry())) {
            // return;
        }
        m_activeWindow = aw;
        reposition();
    }
}

void SharedStatusBar::paintEvent(QPaintEvent *)
{
#ifdef QT3_BUILD
    QPainter p(this);
    p.setPen(QPen(Qt::gray, 1));
#else
    QPainter p(this);
    p.setPen(QColor(160, 160, 160));
#endif
    int x = width() - GRIP_SIZE;
    int y = height() - GRIP_SIZE;
    for (int i = 0; i < 4; i++) {
        int x1 = x + GRIP_SIZE - 3 - i * 4;
        int y1 = y + GRIP_SIZE - i * 4;
        p.drawLine(x1,     y1 + 4, x1 + 4, y1);
        p.drawLine(x1 + 4, y1 + 4, x1 + 8, y1);
    }
}

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
    int barH = m_bar->sizeHint().height();
    if (barH < 20) { barH = 20; }
    resize(m_activeWindow->width(), barH);
    if (!isVisible()) show();
    if (minimumWidth() > m_activeWindow->width()) { m_repositioning = false; return; }

    {
#ifdef Q_OS_LINUX
#ifdef QT3_BUILD
        Display *dpy = QPaintDevice::x11Display();
#else
        Display *dpy = QX11Info::display();
#endif
        XSetTransientForHint(dpy, winId(), m_activeWindow->winId());
        XFlush(dpy);
#endif
    }

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
#ifdef QT3_BUILD
        setCursor(QCursor(isInGripArea(me->pos())
                          ? SizeFDiagCursor : ArrowCursor));
#else
        setCursor(isInGripArea(me->pos())
                  ? Qt::SizeFDiagCursor : Qt::ArrowCursor);
#endif
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
