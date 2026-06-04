// FramelessHelper - Simplified Qt4/Qt3 compatible frameless window helper
#ifndef FRAMELESSHELPER_H
#define FRAMELESSHELPER_H

// Qt3/Qt4 compatibility: Use QApplication::setMainWidget() for Qt3
// Qt3: win.setWFlags(Qt::WType_TopLevel | Qt::WStyle_NoBorder | Qt::WX11BypassWM);
// Qt4: win.setWindowFlags(Qt::FramelessWindowHint);

#ifdef QT3_BUILD
#include <qobject.h>
#include <qwidget.h>
#include <qpoint.h>
#include <qrect.h>
#else
#include <QObject>
#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QMouseEvent>
#endif

#ifdef QT3_BUILD
#define FRAMELESSHELPER_API
#else
#define FRAMELESSHELPER_API Q_DECL_EXPORT
#endif

class FRAMELESSHELPER_API FramelessHelper : public QObject {
    Q_OBJECT
public:
    explicit FramelessHelper(QObject* parent = 0);
    ~FramelessHelper();

    bool setup(QWidget* window);

    // For Qt3, set flags BEFORE calling setup:
    // window->setWFlags(Qt::WType_TopLevel | Qt::WStyle_NoBorder | Qt::WX11BypassWM);
    // helper.setup(window);

    QWidget* titleBar() const;
    void setTitleBar(QWidget* widget);
    
    void setTitleBarButtons(QWidget* sysBtn, QWidget* minBtn, QWidget* maxBtn, QWidget* closeBtn);

    int borderWidth() const;
    void setBorderWidth(int width);

public slots:
    void showSystemMenu(const QPoint& pos);
    void showSystemMenuFromSys();
    void toggleMaximize();
    void onMinClicked();
    void onMaxClicked();
    void onCloseClicked();

signals:
    void titleBarChanged(QWidget* widget);

protected:
    bool eventFilter(QObject* obj, QEvent* event);

private:
    int getResizeRegion(const QPoint& pos) const;

    class Private;
    Private* d;
};

#endif // FRAMELESSHELPER_H