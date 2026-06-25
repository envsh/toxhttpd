#include "unknownparser.h"
#include "cJSON.h"
#include "compat34.h"
#include <dlfcn.h>

// ── JSON 路径导航 ──

static bool isDigits(const std::string& s) {
    return !s.empty() && s.find_first_not_of("0123456789") == std::string::npos;
}

static cJSON* jsonPath(cJSON* root, const char* path) {
    if (!root || !path) return NULL;
    cJSON* cur = root;
    std::string s = path;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t dot = s.find('.', pos);
        std::string key = (dot == std::string::npos) ? s.substr(pos) : s.substr(pos, dot - pos);
        if (cJSON_IsArray(cur) && isDigits(key)) {
            long long idx = std::stoll(key);
            cur = cJSON_GetArrayItem(cur, (int)idx);
        } else {
            cur = cJSON_GetObjectItem(cur, key.c_str());
        }
        if (!cur) return NULL;
        pos = (dot == std::string::npos) ? s.size() : dot + 1;
    }
    return cur;
}

static std::string jsonGetString(cJSON* root, const char* path) {
    cJSON* item = jsonPath(root, path);
    return (item && cJSON_IsString(item)) ? cJSON_GetStringValue(item) : "";
}

static int64_t jsonGetInt64(cJSON* root, const char* path) {
    cJSON* item = jsonPath(root, path);
    return (item && cJSON_IsNumber(item)) ? (int64_t)item->valuedouble : 0;
}

// ── Matrix/gomuks sync_complete 解析 ──

static void parseGomuksEvents(cJSON* roomObj, const std::string& roomId, ParseResult& ret) {
    // 从 events 数组中提取 m.room.member displayname → nickname
    auto scanMembers = [](cJSON* arr, std::vector<PeerInfo>& peers) {
        if (!arr) return;
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; i++) {
            cJSON* ev = cJSON_GetArrayItem(arr, i);
            if (jsonGetString(ev, "type") != "m.room.member") continue;
            std::string sender = jsonGetString(ev, "sender");
            if (sender.empty()) continue;
            std::string dn = jsonGetString(ev, "content.displayname");
            std::string av = jsonGetString(ev, "content.avatar_url");
            if (dn.empty() && av.empty()) continue;

            PeerInfo* pi = nullptr;
            for (auto& p : peers)
                if (p.publicKey == sender) { pi = &p; break; }
            if (!pi) {
                peers.push_back({});
                pi = &peers.back();
                pi->publicKey  = sender;
                pi->name       = sender;
                pi->peerNumber = (int)peers.size() - 1;
            }
            if (!dn.empty()) pi->nickname = dn;
            if (!av.empty()) pi->iconUrl  = av;
        }
    };

    cJSON* events = jsonPath(roomObj, "events");
    if (!events) return;
    int n = cJSON_GetArraySize(events);

    // 从 state.events（room state）+ events（timeline）两个来源提取 nickname
    scanMembers(jsonPath(roomObj, "state.events"), ret.peers);
    scanMembers(events, ret.peers);

    // 消息（仅来自 events）
    for (int i = 0; i < n; i++) {
        cJSON* ev = cJSON_GetArrayItem(events, i);
        if (jsonGetString(ev, "type") != "m.room.message")
            continue;

        HistoryMessage hm;
        hm.message       = jsonGetString(ev, "content.body");
        {
            // ── 媒体消息检测 ──
            std::string msgtype = jsonGetString(ev, "content.msgtype");
            cJSON* info = jsonPath(ev, "content.info");
            if (msgtype.find("m.image") == 0) {
                hm.msgtype  = "image";
                hm.mediaUrl = jsonGetString(ev, "content.url");
                if (info) {
                    hm.mediaWidth  = (int)jsonGetInt64(ev, "content.info.w");
                    hm.mediaHeight = (int)jsonGetInt64(ev, "content.info.h");
                    hm.mediaMime   = jsonGetString(ev, "content.info.mimetype");
                }
            } else if (msgtype.find("m.video") == 0) {
                hm.msgtype      = "video";
                hm.mediaUrl     = jsonGetString(ev, "content.url");
                if (info) {
                    hm.mediaWidth   = (int)jsonGetInt64(ev, "content.info.w");
                    hm.mediaHeight  = (int)jsonGetInt64(ev, "content.info.h");
                    hm.mediaMime    = jsonGetString(ev, "content.info.mimetype");
                    hm.duration     = (int)jsonGetInt64(ev, "content.info.duration");
                    hm.thumbnailUrl = jsonGetString(ev, "content.info.thumbnail_url");
                }
            } else if (msgtype.find("m.audio") == 0) {
                hm.msgtype  = "audio";
                hm.mediaUrl = jsonGetString(ev, "content.url");
                if (info) {
                    hm.mediaMime = jsonGetString(ev, "content.info.mimetype");
                    hm.duration  = (int)jsonGetInt64(ev, "content.info.duration");
                }
            } else if (msgtype.find("m.file") == 0) {
                hm.msgtype  = "file";
                hm.mediaUrl = jsonGetString(ev, "content.url");
                if (info) {
                    hm.mediaMime = jsonGetString(ev, "content.info.mimetype");
                }
            }
        }
        {
            cJSON* content = cJSON_GetObjectItem(ev, "content");
            if (content) {
                std::vector<std::string> replyTexts;
                cJSON* rt = cJSON_GetObjectItem(content, "m.relates_to");
                if (rt) {
                    cJSON* ir = cJSON_GetObjectItem(rt, "m.in_reply_to");
                    if (ir) {
                        cJSON* eid = cJSON_GetObjectItem(ir, "event_id");
                        if (eid && cJSON_IsString(eid))
                            replyTexts.push_back(cJSON_GetStringValue(eid));
                    }
                }
                cJSON* mt = cJSON_GetObjectItem(content, "m.mentions");
                if (mt) {
                    cJSON* uids = cJSON_GetObjectItem(mt, "user_ids");
                    if (uids && cJSON_IsArray(uids)) {
                        std::string ms;
                        int n = cJSON_GetArraySize(uids);
                        for (int j = 0; j < n; j++) {
                            cJSON* uid = cJSON_GetArrayItem(uids, j);
                            if (uid && cJSON_IsString(uid)) {
                                if (!ms.empty()) ms += " ";
                                ms += cJSON_GetStringValue(uid);
                            }
                        }
                        if (!ms.empty())
                            replyTexts.push_back(ms);
                    }
                }
                // 若存在 @ 开头的回复文本，只保留这些
                bool hasAt = false;
                for (const auto& s : replyTexts) {
                    if (!s.empty() && s[0] == '@') { hasAt = true; break; }
                }
                for (const auto& s : replyTexts) {
                    if (hasAt && (s.empty() || s[0] != '@')) continue;
                    hm.message += " -- Re: " + s;
                }
            }
        }
        hm.sender_pubkey = jsonGetString(ev, "sender");
        hm.sender_number = i;
        hm.direction     = "received";
        hm.created_at    = std::to_string(jsonGetInt64(ev, "timestamp"));
        hm.roomId        = roomId;
        ret.messages.push_back(hm);

        bool found = false;
        for (const auto& p : ret.peers) {
            if (p.publicKey == hm.sender_pubkey) {
                found = true;
                break;
            }
        }
        if (!found) {
            PeerInfo pi;
            pi.publicKey  = hm.sender_pubkey;
            pi.name       = hm.sender_pubkey;
            pi.peerNumber = (int)ret.peers.size();
            ret.peers.push_back(pi);
        }
    }
}

static bool tryParseGomuksSync(const std::string& rawStr, ParseResult& ret) {
    if (rawStr.empty()) return false;

    cJSON* root = cJSON_Parse(rawStr.c_str());
    if (!root) return false;

    if (jsonGetString(root, "command") != "sync_complete") {
        cJSON_Delete(root);
        return false;
    }

    cJSON* rooms = jsonPath(root, "data.rooms");
    if (!rooms || !cJSON_IsObject(rooms)) {
        cJSON_Delete(root);
        return false;
    }

    for (cJSON* r = rooms->child; r; r = r->next) {
        if (r->type != cJSON_Object) continue;

        std::string roomId = r->string ? r->string : "";
        ContactData cd;
        cd.id          = (int)(std::hash<std::string>{}(roomId) & 0x7fffffff);
        cd.name        = jsonGetString(r, "meta.name");
        if (cd.name.empty())
            cd.name    = jsonGetString(r, "meta.canonical_alias");
        if (cd.name.empty())
            cd.name    = roomId;
        cd.chatId      = roomId;
        cd.type        = kGomuksRoomType;
        cd.status      = "online";
        cd.isConnected = true;
        ret.contacts.push_back(cd);

        parseGomuksEvents(r, roomId, ret);
    }

    // ret.handled = !ret.contacts.empty() || !ret.peers.empty() || !ret.messages.empty();
    cJSON_Delete(root);
    return true;
}

// ── Tox 消息事件解析 ──

static bool tryParseToxMessage(const std::string& rawStr, ParseResult& ret) {
    cJSON* root = cJSON_Parse(rawStr.c_str());
    if (!root) return false;

    std::string timestamp = jsonGetString(root, "timestamp");
    std::string eventType = jsonGetString(root, "event_type");

    std::string innerDataStr = jsonGetString(root, "data");
    if (!innerDataStr.empty()) {
        cJSON* inner = cJSON_Parse(innerDataStr.c_str());
        if (inner) {
            ContactData cd;
            cd.isConnected = true;
            cd.status = "online";

            if (eventType == "friend_message") {
                int64_t friendId = jsonGetInt64(inner, "friend_id");
                cd.id     = (int)friendId;
                cd.name   = "friend_" + std::to_string(friendId);
                cd.type   = kUnktoxFriendType;
                cd.chatId = std::to_string(friendId);
            } else if (eventType == "conference_message") {
                int64_t confNum = jsonGetInt64(inner, "conference_number");
                cd.id     = (int)confNum;
                cd.name   = "conf_" + std::to_string(confNum);
                cd.type   = kUnktoxConferenceType;
                cd.chatId = std::to_string(confNum);
            } else if (eventType == "group_message") {
                int64_t groupNum = jsonGetInt64(inner, "group_number");
                cd.id     = (int)groupNum;
                cd.name   = "group_" + std::to_string(groupNum);
                cd.type   = kUnktoxGroupType;
                cd.chatId = std::to_string(groupNum);
            }

            if (!cd.chatId.empty()) {
                ret.contactName = qFromUtf8(cd.name);
                ret.contacts.push_back(cd);
            }

            HistoryMessage hm;
            hm.message       = jsonGetString(inner, "message");
            hm.sender_pubkey = jsonGetString(inner, "sender_pubkey");
            hm.sender_number = (uint32_t)jsonGetInt64(inner, "sender");
            hm.direction     = jsonGetString(inner, "direction");
            hm.created_at    = timestamp;
            hm.roomId        = cd.chatId;

            // 创建 peer 信息，供后续显示使用 nickname
            if (!hm.sender_pubkey.empty()) {
                PeerInfo pi;
                pi.publicKey  = hm.sender_pubkey;
                pi.name       = hm.sender_pubkey;
                pi.peerNumber = (int)hm.sender_number;
                std::string peerName = jsonGetString(inner, "peer_name");
                if (!peerName.empty())
                    pi.nickname = peerName;
                ret.peers.push_back(pi);
            }

            if (!hm.message.empty())
                ret.messages.push_back(hm);

            cJSON_Delete(inner);
        }
    }

    ret.handled = !ret.contacts.empty() || !ret.peers.empty() || !ret.messages.empty();
    cJSON_Delete(root);
    return ret.handled;
}

// ── IMAP 邮件解析 ──

// uchardet 动态库检测编码（无外部链接依赖）
static std::string detectEncoding(const QByteArray& data) {
    void* h = dlopen("libuchardet.so.0", RTLD_LAZY);
    if (!h) h = dlopen("libuchardet.so", RTLD_LAZY);
    if (!h) return "";

    auto uchardet_new          = (void*(*)())dlsym(h, "uchardet_new");
    auto uchardet_delete       = (void(*)(void*))dlsym(h, "uchardet_delete");
    auto uchardet_handle_data  = (int(*)(void*,const char*,size_t))dlsym(h, "uchardet_handle_data");
    auto uchardet_data_end     = (void(*)(void*))dlsym(h, "uchardet_data_end");
    auto uchardet_get_charset  = (const char*(*)(void*))dlsym(h, "uchardet_get_charset");
    if (!(uchardet_new && uchardet_delete && uchardet_handle_data &&
          uchardet_data_end && uchardet_get_charset)) {
        dlclose(h);
        return "";
    }

    void* ud = uchardet_new();
    if (!ud) { dlclose(h); return ""; }

    std::string result;
    if (uchardet_handle_data(ud, data.data(), data.size()) == 0) {
        uchardet_data_end(ud);
        const char* cs = uchardet_get_charset(ud);
        if (cs && cs[0]) result = cs;
    }
    uchardet_delete(ud);
    dlclose(h);
    return result;
}

static bool tryParseImapMessage(const std::string& rawStr, ParseResult& ret) {
    cJSON* root = cJSON_Parse(rawStr.c_str());
    if (!root) return false;

    std::string subject    = jsonGetString(root, "subject");
    std::string from       = jsonGetString(root, "from");
    std::string toRecip    = jsonGetString(root, "toRecipients.0");
    std::string bodyB64    = jsonGetString(root, "bodyPreview");
    std::string receivedAt = jsonGetString(root, "receivedDateTime");

    if (subject.empty() && from.empty() && toRecip.empty()) {
        cJSON_Delete(root);
        return false;
    }

    std::string cleanB64;
    for (unsigned char c : bodyB64) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' ||
            c == '=' || c == '-' || c == '_')
            cleanB64 += c;
    }

    std::string fullText = subject;
    fullText += "\n[raw](" + std::to_string(bodyB64.size()) + "): "
             + bodyB64.substr(0, 512) + "\n";
    QByteArray decoded = base64Decode(cleanB64);
    if (!decoded.isEmpty()) {
        fullText += "\n";
        // uchardet 检测结果（如果有）
        std::string enc = detectEncoding(decoded);
        if (!enc.empty()) {
            QTextCodec* codec = QTextCodec::codecForName(enc.c_str());
            if (codec) {
                QString t = codec->toUnicode(decoded);
                if (!t.isEmpty()) {
                    std::string s(qToUtf8(t).data());
                    fullText += std::string("[uchardet] ") + enc + "(" + std::to_string(s.size()) + "): " + s + "\n";
                }
            }
        }
        // 所有候选编码依次解码
        static const char* kCodecs[] = {
            "UTF-8", "GBK", "Shift-JIS", "Big5", "EUC-KR", "ISO-8859-1"
        };
        for (const char* name : kCodecs) {
            QTextCodec* codec = QTextCodec::codecForName(name);
            if (!codec) continue;
            QString t = codec->toUnicode(decoded);
            if (t.isEmpty()) continue;
            std::string s(qToUtf8(t).data());
            fullText += std::string(name) + "(" + std::to_string(s.size()) + "): " + s + "\n";
        }
        if (fullText.size() > 1 && fullText.back() == '\n')
            fullText.pop_back();
    } else if (!cleanB64.empty()) {
        // base64 数据存在但解码后为空 → 解码失败，附上原始 base64 文本
        fullText += "\n(dcode failed, raw: " + cleanB64 + ")";
    }

    ContactData cd;
    cd.id          = (int)(std::hash<std::string>{}(toRecip + kImapMailType) & 0x7fffffff);
    cd.name        = toRecip;
    cd.type        = kImapMailType;
    cd.chatId      = toRecip;
    cd.status      = "online";
    cd.isConnected = true;
    ret.contacts.push_back(cd);

    HistoryMessage hm;
    hm.message       = fullText;
    hm.sender_pubkey = from;
    hm.sender_number = 0;
    hm.direction     = "received";
    hm.created_at    = receivedAt;
    hm.roomId        = cd.chatId;
    ret.messages.push_back(hm);

    // 加入 sender peer 供 ChatView 查找显示名称
    PeerInfo pi;
    pi.publicKey  = from;
    pi.name       = from;
    pi.peerNumber = 0;
    ret.peers.push_back(pi);

    ret.senderName  = qFromUtf8(from);
    if (ret.contactName.isEmpty())
        ret.contactName = qFromUtf8(cd.name);

    ret.handled = true;
    cJSON_Delete(root);
    return true;
}

// ── filesync 事件解析 ──

static bool tryParseFilesyncEvent(const std::string& rawStr, ParseResult& ret) {
    cJSON* root = cJSON_Parse(rawStr.c_str());
    if (!root) return false;

    std::string type   = jsonGetString(root, "type");
    std::string event  = jsonGetString(root, "event");
    std::string path   = jsonGetString(root, "path");
    cJSON_Delete(root);

    if (type != "filesync") return false;

    std::string topic = qToUtf8(ret.contactName).data();
    if (topic.empty()) return false;

    ContactData cd;
    cd.id          = (int)(std::hash<std::string>{}(topic + kFilesyncType) & 0x7fffffff);
    cd.name        = "filesync";
    cd.type        = kFilesyncType;
    cd.chatId      = topic;
    cd.status      = "online";
    cd.isConnected = true;
    ret.contacts.push_back(cd);

    HistoryMessage hm;
    hm.message    = event + ": " + path;
    hm.direction  = "received";
    hm.roomId     = topic;
    ret.messages.push_back(hm);

    ret.handled = true;
    return true;
}

// ── clipboard 事件解析 ──

static bool tryParseClipboardEvent(const std::string& rawStr, ParseResult& ret) {
    cJSON* root = cJSON_Parse(rawStr.c_str());
    if (!root) return false;

    std::string type = jsonGetString(root, "type");
    std::string fmt  = jsonGetString(root, "format");
    std::string data = jsonGetString(root, "data");
    cJSON_Delete(root);

    if (type != "clipboard") return false;

    std::string topic = qToUtf8(ret.contactName).data();
    if (topic.empty()) return false;

    ContactData cd;
    cd.id          = (int)(std::hash<std::string>{}(topic + kClipboardType) & 0x7fffffff);
    cd.name        = "clipboard";
    cd.type        = kClipboardType;
    cd.chatId      = topic;
    cd.status      = "online";
    cd.isConnected = true;
    ret.contacts.push_back(cd);

    HistoryMessage hm;
    hm.message    = data;
    hm.direction  = "received";
    hm.roomId     = topic;
    ret.messages.push_back(hm);

    ret.handled = true;
    return true;
}

// ── 旧逻辑：纯文本降级 ──

static void extractSender(cJSON* valueItem, ParseResult& ret) {
    cJSON* rf = cJSON_GetObjectItem(valueItem, "ReceivedFrom");
    if (!rf || !cJSON_IsString(rf)) {
        ret.senderName = ret.contactName;
        return;
    }
    QString rfStr = qFromUtf8(cJSON_GetStringValue(rf));
    if (rfStr.isEmpty()) {
        ret.senderName = ret.contactName;
        return;
    }
    int peerPos =
#ifdef QT3_BUILD
        rfStr.find("peer.ID ");
#else
        rfStr.indexOf("peer.ID ");
#endif
    if (peerPos >= 0) {
        int start = peerPos + 8;
        int end =
#ifdef QT3_BUILD
            rfStr.find('>', start);
#else
            rfStr.indexOf('>', start);
#endif
        if (end < 0) end = rfStr.length();
        ret.senderName = rfStr.mid(start, end - start);
    } else {
        ret.senderName = rfStr;
    }
}

static void fallbackAsPlainText(cJSON* valueItem, ParseResult& ret) {
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
    extractSender(valueItem, ret);
}

// ── 主流程 ──

ParseResult UnknownParser::parse(const std::string& eventType, const std::string& jsonData) {
    qWarning("UnknownParser::parse: type=[%s] data=[%.980s]", eventType.c_str(), jsonData.c_str());

    if (eventType != "pubsub" && eventType != "unknown") {
        return {false, QString(), QString(), QString()};
    }

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

    if (realType != "pubsub") {
        cJSON_Delete(root);
        return {false, QString(), QString(), QString()};
    }

    ParseResult ret = {false, QString(), QString(), QString()};

    // Topic → contactName
    cJSON* topicItem = cJSON_GetObjectItem(root, "Topic");
    if (!topicItem || !cJSON_IsString(topicItem))
        topicItem = cJSON_GetObjectItem(root, "topic");
    if (topicItem && cJSON_IsString(topicItem)) {
        ret.contactName = qFromUtf8(cJSON_GetStringValue(topicItem));
        qWarning("UnknownParser: found topic=[%s]", qToUtf8(ret.contactName).data());
    } else {
        qWarning("UnknownParser: no topic field found");
    }

    // Value → 路由 Matrix sync / Tox 事件 / 旧逻辑
    cJSON* valueItem = cJSON_GetObjectItem(root, "Value");
    if (valueItem) {
        cJSON* dataItem = cJSON_GetObjectItem(valueItem, "data");
        if (dataItem && cJSON_IsString(dataItem)) {
            const char* dataStr = cJSON_GetStringValue(dataItem);
            if (tryParseGomuksSync(dataStr, ret))
                goto done;
            if (tryParseToxMessage(dataStr, ret))
                goto done;
            if (tryParseImapMessage(dataStr, ret))
                goto done;
            if (tryParseFilesyncEvent(dataStr, ret))
                goto done;
            if (tryParseClipboardEvent(dataStr, ret))
                goto done;
        }
        fallbackAsPlainText(valueItem, ret);
    } else {
        qWarning("UnknownParser: no Value field found");
    }

done:
    qWarning("UnknownParser: parse done (handled=%d contacts=%zu peers=%zu messages=%zu)",
             ret.handled, ret.contacts.size(), ret.peers.size(), ret.messages.size());

    cJSON_Delete(root);
    return ret;
}
