#ifndef SCREENSHOT_OVERLAY_H
#define SCREENSHOT_OVERLAY_H

// Reference:
//   Qt3 QWidget window flags:
//     https://doc.qt.io/archives/3.3/qwidget.html#setWFlags
//   Qt4 QWidget window flags:
//     https://doc.qt.io/archives/4.2/qwidget.html#setWindowFlags
//   KSnapshot RegionGrabber:
//     https://github.com/KDE/ksnapshot/blob/master/src/regiongrabber.cpp

#include "compat34.h"
#include <qwidget.h>
#include <qpixmap.h>
#include <qrect.h>
#include <qpoint.h>
#include <qpainter.h>
#include <qregion.h>
#include <qnamespace.h>

class ScreenshotRegionSelector : public QWidget {
    Q_OBJECT
public:
    ScreenshotRegionSelector(const QPixmap& background);
    ~ScreenshotRegionSelector();

signals:
    void regionSelected(const QRect& rect, const QPixmap& fullPixmap);
    void cancelled();

protected:
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    void paintEvent(QPaintEvent* e);
    void keyPressEvent(QKeyEvent* e);
    void mouseDoubleClickEvent(QMouseEvent* e);

private:
    QPixmap m_background;
    QRect m_selection;
    QPoint m_startPoint;
    bool m_selecting;
    bool m_hasSelection;
};

#endif
