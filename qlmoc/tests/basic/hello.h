#ifndef HELLO_H
#define HELLO_H

#include <qobject.h>
#include <qstring.h>

class Hello : public QObject {
    Q_OBJECT
public:
    Hello(QObject *parent = 0);
signals:
    void greeted(const QString& name);
public slots:
    void greet();
};

#endif
