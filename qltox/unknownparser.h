#ifndef UNKNOWNPARSER_H
#define UNKNOWNPARSER_H

#include <string>
#include <qstring.h>

struct ParseResult {
    bool handled;
    QString contactName;
    QString senderName;
    QString messageText;
};

class UnknownParser {
public:
    static ParseResult parse(const std::string& eventType, const std::string& jsonData);
};

#endif
