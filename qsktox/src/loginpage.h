#ifndef LOGIN_PAGE_H
#define LOGIN_PAGE_H

#include <QskControl.h>

class LoginPage : public QskControl
{
    Q_OBJECT
public:
    LoginPage(QQuickItem* parent = nullptr);

Q_SIGNALS:
    void accepted(const QString& url);
};

#endif
