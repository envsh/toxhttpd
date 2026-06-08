#ifndef UNKNOWNPARSER_H
#define UNKNOWNPARSER_H

#include <string>
#include <vector>
#include "eventpoller.h"

struct ParseResult {
    bool handled;
    QString contactName;
    QString senderName;
    QString messageText;
    std::vector<ContactData> contacts;
    std::vector<PeerInfo> peers;
    std::vector<HistoryMessage> messages;
};

class UnknownParser {
public:
    static ParseResult parse(const std::string& eventType, const std::string& jsonData);
};

#endif
