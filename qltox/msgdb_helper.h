#pragma once
#include "chatview.h"
#include "message_db.h"
#include <functional>

ChatElement msgRowToElement(const MessageRow& row);
typedef std::function<void(int64_t)> WriteMsgCallback;
void db_writeMessage(int id, const std::string& type, const ChatElement& el,
                     WriteMsgCallback cb = nullptr);
