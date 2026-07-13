#include "chatbuffer.h"
#include "assertf.h"

int ChatHistory::m_capacity = 200;

ChatHistory ChatHistory::kEmpty;

ChatHistory& ChatBuffer::getOrCreate(int chatId, const std::string& chatType) {
    return m_map[std::make_pair(chatId, chatType)];
}

void ChatBuffer::append(int chatId, const std::string& chatType, const ChatElement& el) {
    getOrCreate(chatId, chatType).append(el);
}

void ChatBuffer::prepend(int chatId, const std::string& chatType, const std::vector<ChatElement>& els) {
    getOrCreate(chatId, chatType).prepend(els);
}

ChatHistory* ChatBuffer::ptr(int chatId, const std::string& chatType) {
    auto it = m_map.find(std::make_pair(chatId, chatType));
    return (it != m_map.end()) ? &it->second : nullptr;
}

void ChatHistory::append(const ChatElement& el) {
    m_items.push_back(el);
    trimOverflow();
    if (m_observer) {
        m_observer->onInsertOne(m_items.size() - 1);
    }
}

void ChatHistory::prepend(const std::vector<ChatElement>& els) {
    int space = m_capacity - (int)m_items.size();
    if (space <= 0) { return; }
    int toInsert = std::min((int)els.size(), space);
    m_items.insert(m_items.begin(), els.end() - toInsert, els.end());
    if (m_observer && toInsert > 0) {
        m_observer->onInsertRange(0, toInsert);
    }
}

void ChatHistory::trimOverflow() {
    int excess = (int)m_items.size() - m_capacity;
    if (excess <= 0) { return; }
    m_items.erase(m_items.begin(), m_items.begin() + excess);
    if (m_observer) {
        m_observer->onRemoveRange(0, excess);
    }
}
