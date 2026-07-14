#ifndef SCREENSHOT_MANAGER_H
#define SCREENSHOT_MANAGER_H

// Reference:
//   Qt3 QPixmap::grabWindow:
//     https://doc.qt.io/archives/3.3/qpixmap.html#grabWindow
//   Qt4 QPixmap::grabWindow:
//     https://doc.qt.io/archives/4.2/qpixmap.html#grabWindow
//   RegionSelector overlay 设计参考:
//     - KDE KSnapshot: https://github.com/KDE/ksnapshot (regiongrabber.cpp)
//     - Qt Screenshot Example: https://doc.qt.io/qt-4.2/desktop-screenshot.html
//   Active window detection (X11):
//     man XGetInputFocus —
//     https://www.x.org/releases/X11R7.7/doc/man/man3/XGetInputFocus.3.xhtml
//   Temp file 命名模式: qltox/messageinput.cpp:202 现有粘贴处理
//   发送管线: qltox/mainwindow.cpp:2746-2816 onFileSendRequested
//   主题色: qlcomp/StyleParams.h g_activeParams

#include "compat34.h"
#include <qobject.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qtimer.h>
#include <qwidget.h>
#include <qrect.h>

class ScreenshotRegionSelector;

class ScreenshotManager : public QObject {
    Q_OBJECT
public:
    static ScreenshotManager* instance();

    void captureFullScreen();
    void captureActiveWindow();
    void captureRegion();

    void setHideWindow(QWidget* w) { m_hideWindow = w; }
    void setAutoHide(bool b) { m_autoHide = b; }
    void setDelay(int ms) { m_delayMs = ms; }

    QWidget* hideWindow() const { return m_hideWindow; }
    bool autoHide() const { return m_autoHide; }
    int delay() const { return m_delayMs; }

    static QString saveToTempFile(const QPixmap& pixmap);

signals:
    void screenshotReady(const QString& filePath);
    void cancelled();

private slots:
    void doCaptureFullScreen();
    void doCaptureActiveWindow();
    void doCaptureForRegion();
    void onRegionSelected(const QRect& rect, const QPixmap& fullPixmap);

private:
    ScreenshotManager();
    ~ScreenshotManager();

    QPixmap grabScreen();
    void finishCapture(const QPixmap& result);

    QWidget* m_hideWindow;
    bool m_autoHide;
    int m_delayMs;
};

#endif
