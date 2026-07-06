#ifndef ABOUT_PAGE_H
#define ABOUT_PAGE_H

#include <QskControl.h>

class AboutPage : public QskControl
{
    Q_OBJECT
public:
    AboutPage(QQuickItem* parent = nullptr);

Q_SIGNALS:
    void backRequested();
};

#endif
