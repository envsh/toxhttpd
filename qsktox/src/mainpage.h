#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include <QskControl.h>

class MainPage : public QskControl
{
    Q_OBJECT
public:
    MainPage(QQuickItem* parent = nullptr);

Q_SIGNALS:
    void settingsRequested();
    void aboutRequested();
    void logoutRequested();
};

#endif
