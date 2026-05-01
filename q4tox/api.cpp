#include "api.h"
#include "cJSON.h"
#include <QDebug>

ToxAPI::ToxAPI(QObject* parent, const QString& baseUrl) 
    : QObject(parent), baseUrl(baseUrl) {
    manager = new QNetworkAccessManager(this);
}

void ToxAPI::getSelf() {
    get("/api/self", "getSelf");
}

void ToxAPI::setSelfName(const QString& name) {
    post("/api/self/name", QString("name=%1").arg(name), "setSelfName");
}

void ToxAPI::setSelfStatus(const QString& statusMessage) {
    post("/api/self/status", QString("status_message=%1").arg(statusMessage), "setSelfStatus");
}

void ToxAPI::getFriends() {
    get("/api/friends", "getFriends");
}

void ToxAPI::getFriendInfo(int friendId) {
    post("/api/friend", QString("friend_id=%1").arg(friendId), "getFriendInfo");
}

void ToxAPI::addFriend(const QString& publicKey) {
    post("/api/friends", QString("public_key=%1").arg(publicKey), "addFriend");
}

void ToxAPI::deleteFriend(int friendId) {
    post("/api/friend_delete", QString("friend_id=%1").arg(friendId), "deleteFriend");
}

void ToxAPI::sendFriendMessage(int friendId, const QString& message) {
    QString data = QString("friend_id=%1&message=%2").arg(friendId).arg(message);
    post("/api/messages", data, "sendFriendMessage");
}

void ToxAPI::getConferences() {
    get("/api/conferences", "getConferences");
}

void ToxAPI::createConference() {
    post("/api/conferences", "", "createConference");
}

void ToxAPI::joinConference(int friendNumber, const QString& cookie) {
    QString data = QString("friend_number=%1&cookie=%2").arg(friendNumber).arg(cookie);
    post("/api/conferences/join", data, "joinConference");
}

void ToxAPI::rejectConference(int friendNumber) {
    QString data = QString("friend_number=%1").arg(friendNumber);
    post("/api/conferences/reject", data, "rejectConference");
}

void ToxAPI::ignoreConference(int friendNumber) {
    QString data = QString("friend_number=%1").arg(friendNumber);
    post("/api/conferences/ignore", data, "ignoreConference");
}

void ToxAPI::sendConferenceMessage(int conferenceId, const QString& message) {
    QString data = QString("conference_id=%1&message=%2").arg(conferenceId).arg(message);
    post("/api/conference_messages", data, "sendConferenceMessage");
}

void ToxAPI::getGroups() {
    get("/api/groups", "getGroups");
}

void ToxAPI::createGroup() {
    post("/api/groups", "", "createGroup");
}

void ToxAPI::sendGroupMessage(int groupId, const QString& message) {
    QString data = QString("group_id=%1&message=%2").arg(groupId).arg(message);
    post("/api/group_messages", data, "sendGroupMessage");
}

void ToxAPI::pollEvents(quint64 after) {
    QString endpoint = QString("/api/events?after=%1").arg(after);
    get(endpoint, "pollEvents");
}

void ToxAPI::bootstrap() {
    post("/api/bootstrap", "", "bootstrap");
}

void ToxAPI::get(const QString& endpoint, const QString& callbackName) {
    QNetworkRequest request(QUrl(baseUrl + endpoint));
    QNetworkReply* reply = manager->get(request);
    reply->setProperty("callback", callbackName);
    connect(reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
}

void ToxAPI::post(const QString& endpoint, const QString& postData, const QString& callbackName) {
    QNetworkRequest request(QUrl(baseUrl + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    
    QByteArray data = postData.toUtf8();
    QNetworkReply* reply = manager->post(request, data);
    reply->setProperty("callback", callbackName);
    connect(reply, SIGNAL(finished()), this, SLOT(onReplyFinished()));
}

void ToxAPI::onReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    QString callback = reply->property("callback").toString();
    QByteArray responseData = reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    
    if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
        emit errorOccurred(QString("HTTP %1: %2").arg(statusCode).arg(reply->errorString()));
        reply->deleteLater();
        return;
    }
    
    if (callback == "getSelf") {
        QVariantMap data = parseJsonResponse(responseData);
        emit selfLoaded(data);
    } else if (callback == "getFriends") {
        cJSON* root = cJSON_Parse(responseData.constData());
        if (root) {
            QList<int> ids;
            if (cJSON_IsArray(root)) {
                for (int i = 0; i < cJSON_GetArraySize(root); i++) {
                    cJSON* item = cJSON_GetArrayItem(root, i);
                    if (cJSON_IsNumber(item)) {
                        ids.append(item->valueint);
                    }
                }
            }
            cJSON_Delete(root);
            emit friendsLoaded(ids);
        }
    } else if (callback == "getFriendInfo") {
        QVariantMap data = parseJsonResponse(responseData);
        FriendInfo info;
        info.id = data["friend_id"].toInt();
        info.name = data["name"].toString();
        info.statusMessage = data["status_message"].toString();
        info.status = data["status"].toString();
        info.connectionStatus = data["connection_status"].toString();
        info.publicKey = data["public_key"].toString();
        emit friendInfoLoaded(info);
    } else if (callback == "sendFriendMessage") {
        emit messageSent(true);
    } else if (callback == "getConferences") {
        cJSON* root = cJSON_Parse(responseData.constData());
        if (root) {
            QList<int> ids;
            if (cJSON_IsArray(root)) {
                for (int i = 0; i < cJSON_GetArraySize(root); i++) {
                    cJSON* item = cJSON_GetArrayItem(root, i);
                    if (cJSON_IsNumber(item)) {
                        ids.append(item->valueint);
                    }
                }
            }
            cJSON_Delete(root);
            emit conferencesLoaded(ids);
        }
    } else if (callback == "pollEvents") {
        EventList events = parseEvents(responseData);
        emit eventsReceived(events);
    }
    
    reply->deleteLater();
}

QVariantMap ToxAPI::parseJsonResponse(const QByteArray& data) {
    return parseJsonString(QString::fromUtf8(data));
}

QVariantMap ToxAPI::parseJsonString(const QString& jsonStr) {
    QVariantMap result;
    cJSON* root = cJSON_Parse(jsonStr.toUtf8().constData());
    if (!root) return result;
    
    if (cJSON_IsObject(root)) {
        cJSON* child = root->child;
        while (child) {
            QString key = QString::fromUtf8(child->string);
            if (cJSON_IsString(child)) {
                result[key] = QString::fromUtf8(cJSON_GetStringValue(child));
            } else if (cJSON_IsNumber(child)) {
                result[key] = child->valueint;
            } else if (cJSON_IsBool(child)) {
                result[key] = child->valueint != 0;
            }
            child = child->next;
        }
    }
    
    cJSON_Delete(root);
    return result;
}

EventList ToxAPI::parseEvents(const QByteArray& data) {
    EventList events;
    cJSON* root = cJSON_Parse(data.constData());
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return events;
    }
    
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(item)) continue;
        
        Event event;
        
        cJSON* idItem = cJSON_GetObjectItem(item, "event_id");
        if (idItem && cJSON_IsNumber(idItem)) {
            event.id = idItem->valueint;
        }
        
        cJSON* typeItem = cJSON_GetObjectItem(item, "event_type");
        if (typeItem && cJSON_IsString(typeItem)) {
            event.type = QString::fromUtf8(cJSON_GetStringValue(typeItem));
        }
        
        cJSON* dataItem = cJSON_GetObjectItem(item, "data");
        if (dataItem && cJSON_IsString(dataItem)) {
            event.data = QString::fromUtf8(cJSON_GetStringValue(dataItem));
        }
        
        events.append(event);
    }
    
    cJSON_Delete(root);
    return events;
}
