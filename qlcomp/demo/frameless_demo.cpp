#include "../FramelessHelper.h"

#ifdef QT3_BUILD
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qlayout.h>
#include <qfont.h>
#else
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QVBoxLayout>
#endif

#ifdef QT3_BUILD
class FramelessWindow : public QWidget {
public:
    FramelessWindow() : QWidget(0, 0, (Qt::WFlags)(Qt::WType_TopLevel | Qt::WStyle_Customize | Qt::WSubWindow | Qt::WStyle_MinMax | Qt::WStyle_SysMenu | Qt::WStyle_NoBorder)) {
        setCaption("Frameless Window");
    }
};
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
#ifdef QT3_BUILD
    QFont f = app.font();
    f.setPointSize(12);
    app.setFont(f);
    
    FramelessWindow* win = new FramelessWindow();
    QWidget* winPtr = win;
#else
    QWidget win;
    win.setWindowTitle("Frameless Window");
    win.setWindowFlags(Qt::FramelessWindowHint);
    win.setMinimumSize(400, 300);
    win.move(100, 100);
    QWidget* winPtr = &win;
#endif
    
#ifdef QT3_BUILD
    winPtr->setMinimumSize(400, 300);
    winPtr->move(100, 100);
#endif
    
    QVBoxLayout* vlayout = new QVBoxLayout(winPtr);
    vlayout->setSpacing(0);
    vlayout->setMargin(0);
    
    QWidget* titleBar = new QWidget(winPtr);
    titleBar->setMinimumHeight(30);
    titleBar->setMaximumHeight(30);
    
    QHBoxLayout* hlayout = new QHBoxLayout(titleBar);
    hlayout->setSpacing(1);
    hlayout->setMargin(1);
    
    QPushButton* sysBtn = new QPushButton("M", titleBar);
    sysBtn->setFixedSize(25, 30);
    
    QLabel* titleLabel = new QLabel("Frameless Window", titleBar);
    titleLabel->setAlignment(Qt::AlignCenter);
    
    QPushButton* minBtn = new QPushButton("_", titleBar);
    minBtn->setFixedSize(30, 30);
    
    QPushButton* maxBtn = new QPushButton("O", titleBar);
    maxBtn->setFixedSize(30, 30);
    
    QPushButton* closeBtn = new QPushButton("X", titleBar);
    closeBtn->setFixedSize(30, 30);
    
    hlayout->addWidget(sysBtn, 0);
    hlayout->addWidget(titleLabel, 1);
    hlayout->addWidget(minBtn, 0);
    hlayout->addWidget(maxBtn, 0);
    hlayout->addWidget(closeBtn, 0);
    
    vlayout->addWidget(titleBar, 0);
    
    QLabel* content = new QLabel("Main Content\n\nClick and drag title bar to move\nDouble click title bar to maximize\nPress X to close", winPtr);
    content->setAlignment(Qt::AlignCenter);
    vlayout->addWidget(content, 1);
    
    FramelessHelper helper(winPtr);
    helper.setup(winPtr);
    helper.setTitleBar(titleBar);
    
#ifdef QT3_BUILD
    QObject::connect(closeBtn, SIGNAL(clicked()), winPtr, SLOT(close()));
    QObject::connect(minBtn, SIGNAL(clicked()), winPtr, SLOT(showMinimized()));
    QObject::connect(maxBtn, SIGNAL(clicked()), &helper, SLOT(toggleMaximize()));
#else
    helper.setTitleBarButtons(sysBtn, minBtn, maxBtn, closeBtn);
#endif
    
    winPtr->show();
    
    return app.exec();
}
