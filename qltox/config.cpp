#include "config.h"
#include "compat34.h"
#include "hjson_wrap.h"
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
    auto res = hjsonLoad(configFilePath());
    if (res.isErr()) { return QString(); }
    Hjson::Value item = res.unwrap()[qToUtf8(key).data()];
    if (!item.defined()) { return QString(); }
    return qFromUtf8(item.to_string());
}

bool Config::setValue(const QString& key, const QString& value) {
    auto res = hjsonLoad(configFilePath());
    Hjson::Value root = res.isOk()
        ? std::move(res.unwrap())
        : Hjson::Value(Hjson::Type::Map);
    root[qToUtf8(key).data()] = qToUtf8(value).data();
    return hjsonSave(root, configFilePath()).isOk();
}
