#ifndef PUSH_HANDLER_H
#define PUSH_HANDLER_H

#include <QObject>

class PushHandler : public QObject
{
    Q_OBJECT
public:
    static void start();
    static void stop();

Q_SIGNALS:
    void pushReceived(const QString& endpoint, const QString& instance);
    void pushMessage(const QByteArray& message, const QString& instance);
};

#endif
