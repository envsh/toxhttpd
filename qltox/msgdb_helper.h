#pragma once
#include "chatview.h"
#include "message_db.h"

ChatElement msgRowToElement(const MessageRow& row);
void db_writeMessage(int id, const std::string& type, const ChatElement& el);
