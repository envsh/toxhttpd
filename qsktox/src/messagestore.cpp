#include "messagestore.h"

MessageStore* MessageStore::s_instance = nullptr;

MessageStore::MessageStore(QObject* parent)
    : QObject(parent)
{
}

MessageStore* MessageStore::instance()
{
    if (!s_instance) {
        s_instance = new MessageStore();
    }
    return s_instance;
}

void MessageStore::addMessage(const QString& chatId, const MessageItem& item)
{
    auto& msgs = m_channelMessages[chatId];
    msgs.append(item);
    while (msgs.size() > MAX_MESSAGES_PER_CHANNEL) {
        msgs.removeFirst();
    }
    emit messageAdded(chatId, item);
}

QList<MessageItem> MessageStore::getMessages(const QString& chatId) const
{
    return m_channelMessages.value(chatId);
}

int MessageStore::messageCount(const QString& chatId) const
{
    return m_channelMessages.value(chatId).size();
}
