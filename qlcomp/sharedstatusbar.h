#ifndef SHAREDSTATUSBAR_H
#define SHAREDSTATUSBAR_H

// Compatible with Qt3 (buildqt3.sh) and Qt4 (buildqt4.sh)

#ifdef QT3_BUILD
#include <qwidget.h>
#include <qstatusbar.h>
#include <qapplication.h>
#include <qrect.h>
#include <qpoint.h>
#include <qlayout.h>
#include <qtimer.h>
#include <X11/Xlib.h>
#else
#include <QWidget>
#include <QStatusBar>
#include <QMouseEvent>
#include <QApplication>
#include <QVBoxLayout>
#include <QTimer>
#include <QX11Info>
#include <X11/Xlib.h>
#endif

class SharedStatusBar : public QWidget
{
    Q_OBJECT
public:
    static SharedStatusBar *instance();

    void showMessage(const QString &msg, int timeout = 0);
    void clearMessage();
    void addWidget(QWidget *w, int stretch = 0);
    void addPermanentWidget(QWidget *w, int stretch = 0);
    void removeWidget(QWidget *w);

private:
    SharedStatusBar();
    ~SharedStatusBar();
    static SharedStatusBar *s_instance;

    QStatusBar *m_bar;
    QWidget    *m_activeWindow;
    bool        m_dragging;
    bool        m_repositioning;
    QPoint      m_dragStartGlobal;
    QRect       m_windowStartGeo;

    bool isInGripArea(const QPoint &localPos) const;
    void handleGripPress(const QPoint &globalPos);
    void handleGripDrag(const QPoint &globalPos);
    void handleGripRelease();
    void reposition();
    void stackAboveWindow(WId targetWin);

    void paintEvent(QPaintEvent *e) override;
    bool event(QEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

#ifdef QT3_BUILD
private slots:
    void onDebounceTimeout();

private:
    QTimer *m_debounceTimer;
    bool m_pendingHide;
#endif

private slots:
    void retrack();
};

#endif
