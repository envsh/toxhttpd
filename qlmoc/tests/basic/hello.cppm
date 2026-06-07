module;
#include <qobject.h>
#include <qstring.h>

export module hellomod;

export class Hello : public QObject {
    Q_OBJECT
public:
    Hello(QObject *parent = 0);
signals:
    void greeted(const QString& name);
public slots:
    void greet();
};
