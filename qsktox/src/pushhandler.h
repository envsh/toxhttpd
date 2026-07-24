#ifndef PUSH_HANDLER_H
#define PUSH_HANDLER_H

#include <QObject>
#include <QStringList>

class PushHandler : public QObject
{
    Q_OBJECT
public:
    static void start();
    static void stop();
    static PushHandler* instance();
    static bool isNtfyInstalled();
    static bool isConnected();
    static bool isRegistering();
    void setConnected(bool v);
    void setRegistering(bool v);

    void registerDevice();
    void selectDistributor(const QString& distributor);
    void cancelRegistrationTimeout();

Q_SIGNALS:
    void distributorsFound(const QStringList& distributors);
    void pushReceived(const QString& endpoint, const QString& instance);
    void pushMessage(const QByteArray& message, const QString& instance);
    void registrationFailed(const QString& reason);
    void registrationSent();
    void statusChanged();

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void startRegistrationTimeout();
    int m_regTimeoutTimerId = 0;
    bool m_isConnected = false;
    bool m_isRegistering = false;
};

#endif
