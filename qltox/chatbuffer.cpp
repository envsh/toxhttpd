#include "chatbuffer.h"

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
}

void ChatHistory::prepend(const std::vector<ChatElement>& els) {
    int space = m_capacity - (int)m_items.size();
    if (space <= 0) { return; }
    int toInsert = std::min((int)els.size(), space);
    m_items.insert(m_items.begin(), els.end() - toInsert, els.end());
}

void ChatHistory::trimOverflow() {
    while ((int)m_items.size() > m_capacity) {
        m_items.pop_front();
    }
}
