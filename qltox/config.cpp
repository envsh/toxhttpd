#include "config.h"
#include "compat34.h"
#ifdef QT3_BUILD
#include <qdir.h>
#else
#include <QDir>
#endif
#include <fstream>
#include <sstream>
#include <cstdlib>

static std::string qStrToStd(const QString& s) {
    return std::string(qToUtf8(s).data());
}

static QString configDir() {
#ifdef QT3_BUILD
    return QDir::homeDirPath() + "/.config/qltox";
#else
    return QDir::homePath() + "/.config/qltox";
#endif
}

static std::string configFilePath() {
    return qStrToStd(configDir()) + "/config.json";
}

static cJSON* loadConfig() {
    std::string path = configFilePath();
    std::ifstream ifs(path);
    if (!ifs.is_open()) { return nullptr; }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return cJSON_Parse(ss.str().c_str());
}

static bool saveConfig(cJSON* root) {
    std::string path = configFilePath();
    std::string dir = path.substr(0, path.rfind('/'));
    qMkdir(qFromUtf8(dir));
    char* jsonStr = cJSON_Print(root);
    std::ofstream ofs(path);
    bool ok = ofs.is_open();
    if (ok) { ofs << jsonStr; }
    free(jsonStr);
    return ok;
}

cJSON* Config::loadRoot() {
    return loadConfig();
}

bool Config::saveRoot(cJSON* root) {
    return saveConfig(root);
}

QString Config::value(const QString& key) {
    cJSON* root = loadConfig();
    if (!root) { return QString(); }
    cJSON* item = cJSON_GetObjectItem(root, qToUtf8(key).data());
    QString val = (item && cJSON_IsString(item)) ? qFromUtf8(item->valuestring) : QString();
    cJSON_Delete(root);
    return val;
}

bool Config::setValue(const QString& key, const QString& value) {
    cJSON* root = loadConfig();
    if (!root) { root = cJSON_CreateObject(); }
    cJSON_DeleteItemFromObject(root, qToUtf8(key).data());
    cJSON_AddStringToObject(root, qToUtf8(key).data(), qToUtf8(value).data());
    bool ok = saveConfig(root);
    cJSON_Delete(root);
    return ok;
}
