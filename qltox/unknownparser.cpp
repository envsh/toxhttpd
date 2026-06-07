#include "unknownparser.h"
#include "cJSON.h"
#include "compat34.h"

ParseResult UnknownParser::parse(const std::string& eventType, const std::string& jsonData) {
    qWarning("UnknownParser::parse: type=[%s] data=[%.160s]", eventType.c_str(), jsonData.c_str());

    if (eventType == "pubsub" || eventType == "unknown") {
        cJSON* root = cJSON_Parse(jsonData.c_str());
        if (!root) {
            qWarning("UnknownParser: cJSON_Parse failed");
            return {false, QString(), QString(), QString()};
        }

        std::string realType = eventType;
        if (eventType == "unknown") {
            cJSON* typeItem = cJSON_GetObjectItem(root, "Type");
            if (typeItem && cJSON_IsString(typeItem))
                realType = cJSON_GetStringValue(typeItem);
        }

            if (realType == "pubsub") {
            ParseResult ret = {false, QString(), QString(), QString()};

            cJSON* topicItem = cJSON_GetObjectItem(root, "Topic");
            if (!topicItem || !cJSON_IsString(topicItem))
                topicItem = cJSON_GetObjectItem(root, "topic");
            if (topicItem && cJSON_IsString(topicItem)) {
                ret.contactName = qFromUtf8(cJSON_GetStringValue(topicItem));
                qWarning("UnknownParser: found topic=[%s]", qToUtf8(ret.contactName).data());
            } else {
                qWarning("UnknownParser: no topic field found");
            }

            cJSON* valueItem = cJSON_GetObjectItem(root, "Value");
            if (valueItem) {
                cJSON* dataItem = cJSON_GetObjectItem(valueItem, "data");
                if (dataItem && cJSON_IsString(dataItem)) {
                    ret.messageText = qFromUtf8(cJSON_GetStringValue(dataItem));
                    qWarning("UnknownParser: extracted Value.data, len=%d", ret.messageText.length());
                } else {
                    char* raw = cJSON_PrintUnformatted(valueItem);
                    ret.messageText = qFromUtf8(raw);
                    qWarning("UnknownParser: no Value.data, fell back to Value JSON, len=%d", ret.messageText.length());
                    free(raw);
                }
            } else {
                qWarning("UnknownParser: no Value field found");
            }

            // 从 Value.ReceivedFrom 提取发送者短 ID
            if (valueItem) {
                cJSON* receivedFrom = cJSON_GetObjectItem(valueItem, "ReceivedFrom");
                if (receivedFrom && cJSON_IsString(receivedFrom)) {
                    QString rf = qFromUtf8(cJSON_GetStringValue(receivedFrom));
                    // 格式: "<peer.ID 12D3KooW...>"
                    int peerPos =
#ifdef QT3_BUILD
                        rf.find("peer.ID ");
#else
                        rf.indexOf("peer.ID ");
#endif
                    if (peerPos >= 0) {
                        int start = peerPos + 8; // 跳过 "peer.ID "
                        int end =
#ifdef QT3_BUILD
                            rf.find('>', start);
#else
                            rf.indexOf('>', start);
#endif
                        if (end < 0) end = rf.length();
                        ret.senderName = rf.mid(start, end - start);
                    } else {
                        ret.senderName = rf;
                    }
                }
            }
            if (ret.senderName.isEmpty())
                ret.senderName = ret.contactName;

            if (!ret.contactName.isEmpty())
                ret.handled = true;

            qWarning("UnknownParser: pubsub parse complete, handled=%d, contact=[%s], sender=[%s]", ret.handled, qToUtf8(ret.contactName).data(), qToUtf8(ret.senderName).data());

            cJSON_Delete(root);
            return ret;
        }

        cJSON_Delete(root);
    }

    qWarning("UnknownParser: unhandled event type [%s]", eventType.c_str());
    return {false, QString(), QString(), QString()};
}
