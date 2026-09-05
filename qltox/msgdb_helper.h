#pragma once
#include "chatview.h"
#include "message_db.h"
#include <functional>

ChatElement msgRowToElement(const MessageRow& row);
typedef std::function<void(int64_t)> WriteMsgCallback;
void db_writeMessage(int id, const std::string& type, const ChatElement& el,
                     WriteMsgCallback cb = nullptr);

// 注册行号回填事件的目标（主线程 QObject，写队列线程通过 postEvent 投递）。
void setRowidEventTarget(QObject* target);
