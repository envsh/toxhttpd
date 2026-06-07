// FramelessHelper - Qt4/Qt3 compatible implementation
#include "FramelessHelper.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#include <qobject.h>
#include <qwidget.h>
#include <qevent.h>
#include <qpopupmenu.h>
#include <qpoint.h>
#include <qrect.h>
#include <qcursor.h>
#else
#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QEvent>
#include <QMenu>
#include <QDebug>
#include <QPoint>
#include <QRect>
#include <QCursor>
#endif

class FramelessHelper::Private {
public:
    Private() : hostWindow(0), titleBarWidget(0), borderWidth(8), isMousePressed(false), isMouseMoved(false), resizeRegion(0) {}

    QWidget* hostWindow;
    QWidget* titleBarWidget;
    int borderWidth;
    QPoint mousePressPos;
    bool isMousePressed;
    bool isMouseMoved;
    int resizeRegion;
};

FramelessHelper::FramelessHelper(QObject* parent)
    : QObject(parent)
    , d(new Private())
{
}

FramelessHelper::~FramelessHelper() {
    delete d;
}

bool FramelessHelper::setup(QWidget* window) {
    if (!window) {
        return false;
    }
    if (d->hostWindow) {
        return false;
    }
    d->hostWindow = window;
    window->setMouseTracking(true);
    window->installEventFilter(this);
    return true;
}

QWidget* FramelessHelper::titleBar() const {
    return d->titleBarWidget;
}

void FramelessHelper::setTitleBar(QWidget* widget) {
    d->titleBarWidget = widget;
    emit titleBarChanged(widget);
}

void FramelessHelper::setTitleBarButtons(QWidget* sysBtn, QWidget* minBtn, QWidget* maxBtn, QWidget* closeBtn) {
    if (sysBtn) {
        QObject::connect(sysBtn, SIGNAL(clicked()), this, SLOT(showSystemMenuFromSys()));
    }
    if (minBtn) {
        QObject::connect(minBtn, SIGNAL(clicked()), this, SLOT(onMinClicked()));
    }
    if (maxBtn) {
        QObject::connect(maxBtn, SIGNAL(clicked()), this, SLOT(onMaxClicked()));
    }
    if (closeBtn) {
        QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(onCloseClicked()));
    }
}

int FramelessHelper::borderWidth() const {
    return d->borderWidth;
}

void FramelessHelper::setBorderWidth(int width) {
    d->borderWidth = width;
}

void FramelessHelper::showSystemMenu(const QPoint& pos) {
    if (!d->hostWindow) {
        return;
    }

#ifdef QT3_BUILD
    QPopupMenu menu(0);
    menu.insertItem(QString::fromUtf8("还原(&R)"), d->hostWindow, SLOT(showNormal()));
    menu.insertItem(QString::fromUtf8("最大化(&X)"), d->hostWindow, SLOT(showMaximized()));
    menu.insertItem(QString::fromUtf8("最小化(&N)"), d->hostWindow, SLOT(showMinimized()));
    menu.insertSeparator();
    menu.insertItem(QString::fromUtf8("关闭(&C)"), d->hostWindow, SLOT(close()));
    menu.exec(pos);
#else
    QMenu* menu = new QMenu(d->hostWindow);
    QAction* a1 = menu->addAction(QString::fromUtf8("还原(&R)"));
    QObject::connect(a1, SIGNAL(triggered()), d->hostWindow, SLOT(showNormal()));
    QAction* a2 = menu->addAction(QString::fromUtf8("最大化(&X)"));
    QObject::connect(a2, SIGNAL(triggered()), d->hostWindow, SLOT(showMaximized()));
    QAction* a3 = menu->addAction(QString::fromUtf8("最小化(&N)"));
    QObject::connect(a3, SIGNAL(triggered()), d->hostWindow, SLOT(showMinimized()));
    menu->addSeparator();
    QAction* a4 = menu->addAction(QString::fromUtf8("关闭(&C)"));
    QObject::connect(a4, SIGNAL(triggered()), d->hostWindow, SLOT(close()));
    menu->exec(pos);
#endif
}

void FramelessHelper::showSystemMenuFromSys() {
    if (!d->titleBarWidget || !d->hostWindow) {
        return;
    }
    QPoint pos = d->titleBarWidget->mapToGlobal(QPoint(d->titleBarWidget->width() - 100, d->titleBarWidget->height()));
    showSystemMenu(pos);
}

void FramelessHelper::toggleMaximize() {
    if (!d->hostWindow) {
        return;
    }
#ifdef QT3_BUILD
    if (d->hostWindow->isMaximized()) {
        d->hostWindow->showNormal();
    } else {
        d->hostWindow->showMaximized();
    }
#else
    if (d->hostWindow->windowState() & Qt::WindowMaximized) {
        d->hostWindow->showNormal();
    } else {
        d->hostWindow->showMaximized();
    }
#endif
}

void FramelessHelper::onMinClicked() {
    if (!d->hostWindow) {
        return;
    }
    d->hostWindow->showMinimized();
}

void FramelessHelper::onMaxClicked() {
    toggleMaximize();
}

void FramelessHelper::onCloseClicked() {
    if (!d->hostWindow) {
        return;
    }
    d->hostWindow->close();
}

bool FramelessHelper::eventFilter(QObject* obj, QEvent* event) {
    if (!d->hostWindow || obj != d->hostWindow) {
        return false;
    }

    QEvent::Type type = event->type();

    if (type == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton) {
            return false;
        }
        QPoint pos = me->globalPos();

        if (d->titleBarWidget) {
            QPoint localPos = d->hostWindow->mapFromGlobal(pos);
            if (d->titleBarWidget->geometry().contains(localPos)) {
                d->isMousePressed = true;
                d->mousePressPos = pos;
                d->isMouseMoved = false;
                return true;
            }
        }

        int region = getResizeRegion(pos);
        if (region != 0) {
            d->isMousePressed = true;
            d->resizeRegion = region;
            d->mousePressPos = pos;
            return true;
        }
    }

    if (type == QEvent::MouseMove && d->isMousePressed) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QPoint pos = me->globalPos();

        if (d->resizeRegion != 0) {
            QRect geo = d->hostWindow->geometry();
            int dx = pos.x() - d->mousePressPos.x();
            int dy = pos.y() - d->mousePressPos.y();

            switch (d->resizeRegion) {
                case 1: geo.setLeft(geo.left() + dx); break;
                case 2: geo.setTop(geo.top() + dy); break;
                case 4: geo.setRight(geo.right() + dx); break;
                case 8: geo.setBottom(geo.bottom() + dy); break;
                case 3: geo.setLeft(geo.left() + dx); geo.setTop(geo.top() + dy); break;
                case 5: geo.setRight(geo.right() + dx); geo.setTop(geo.top() + dy); break;
                case 6: geo.setLeft(geo.left() + dx); geo.setBottom(geo.bottom() + dy); break;
                case 9: geo.setRight(geo.right() + dx); geo.setBottom(geo.bottom() + dy); break;
            }

            d->hostWindow->setGeometry(geo);
            d->mousePressPos = pos;
            return true;
        } else {
            QPoint delta = pos - d->mousePressPos;
            d->hostWindow->move(d->hostWindow->x() + delta.x(), d->hostWindow->y() + delta.y());
            d->mousePressPos = pos;
            d->isMouseMoved = true;
            return true;
        }
    }

    if (type == QEvent::MouseButtonRelease) {
        d->isMousePressed = false;
        d->isMouseMoved = false;
        d->resizeRegion = 0;
        d->hostWindow->unsetCursor();
    }

    if (type == QEvent::MouseButtonDblClick && d->titleBarWidget) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        QPoint localPos = d->hostWindow->mapFromGlobal(me->globalPos());
        if (d->titleBarWidget->geometry().contains(localPos)) {
#ifdef QT3_BUILD
            if (d->hostWindow->isMaximized()) {
                d->hostWindow->showNormal();
            } else {
                d->hostWindow->showMaximized();
            }
#else
            toggleMaximize();
#endif
            return true;
        }
    }

    if (type == QEvent::ContextMenu) {
        QContextMenuEvent* ce = static_cast<QContextMenuEvent*>(event);
        showSystemMenu(ce->globalPos());
        return true;
    }

    if (type == QEvent::MouseMove && !d->isMousePressed) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        int region = getResizeRegion(me->globalPos());
        switch (region) {
            case 1:
            case 4:
                d->hostWindow->setCursor(Qt::SizeHorCursor);
                break;
            case 2:
            case 8:
                d->hostWindow->setCursor(Qt::SizeVerCursor);
                break;
            case 3:
            case 6:
                d->hostWindow->setCursor(Qt::SizeBDiagCursor);
                break;
            case 5:
            case 9:
                d->hostWindow->setCursor(Qt::SizeFDiagCursor);
                break;
            default:
                d->hostWindow->setCursor(QCursor(Qt::ArrowCursor));
                break;
        }
    }

    return false;
}

int FramelessHelper::getResizeRegion(const QPoint& pos) const {
    if (!d->hostWindow) {
        return 0;
    }

    QRect geo = d->hostWindow->geometry();
    int bw = d->borderWidth;

    int x = pos.x();
    int y = pos.y();
    int left = geo.left();
    int right = geo.right();
    int top = geo.top();
    int bottom = geo.bottom();

    bool onLeft = (x >= left - bw && x < left + bw);
    bool onRight = (x >= right - bw && x < right + bw);
    bool onTop = (y >= top - bw && y < top + bw);
    bool onBottom = (y >= bottom - bw && y < bottom + bw);

    int region = 0;
    if (onLeft) { region |= 1; }
    if (onTop) { region |= 2; }
    if (onRight) { region |= 4; }
    if (onBottom) { region |= 8; }

    return region;
}