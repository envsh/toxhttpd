#ifndef MESSAGE_STORE_H
#define MESSAGE_STORE_H

#include "messagelist.h"
#include <QObject>
#include <QMap>
#include <QList>

class MessageStore : public QObject
{
    Q_OBJECT
public:
    static MessageStore* instance();

    static constexpr int MAX_MESSAGES_PER_CHANNEL = 80;
    static constexpr int MAX_CHANNELS = 200;

    void addMessage(const QString& chatId, const MessageItem& item);
    QList<MessageItem> getMessages(const QString& chatId) const;
    int messageCount(const QString& chatId) const;

Q_SIGNALS:
    void messageAdded(const QString& chatId, const MessageItem& item);

private:
    MessageStore(QObject* parent = nullptr);
    static MessageStore* s_instance;
    QMap<QString, QList<MessageItem>> m_channelMessages;
};

#endif
