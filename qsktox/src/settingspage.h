#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include <QskControl.h>

class SettingsPage : public QskControl
{
    Q_OBJECT
public:
    SettingsPage(QQuickItem* parent = nullptr);

Q_SIGNALS:
    void backRequested();
};

#endif
