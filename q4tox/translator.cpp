#include "translator.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QTextCodec>

Translator::Translator(QObject* parent) : QObject(parent), m_root(0) {
}

Translator::~Translator() {
    if (m_root) {
        cJSON_Delete((cJSON*)m_root);
    }
}

Translator& Translator::instance() {
    static Translator instance;
    return instance;
}

QString Translator::getExeDir() const {
    // In Qt4, use QCoreApplication::applicationFilePath()
    QString exePath = QCoreApplication::applicationFilePath();
    QFileInfo fi(exePath);
    return fi.absolutePath() + "/";
}

bool Translator::loadLanguage(const QString& langCode) {
    // Try multiple paths
    QStringList paths;
    paths.append(getExeDir() + "lang/" + langCode + ".json");
    paths.append("lang/" + langCode + ".json");
    paths.append("/home/gzleo/aprog/toxhttpd/q4tox/lang/" + langCode + ".json");
    paths.append("/usr/local/share/q4tox/lang/" + langCode + ".json");
    
    for (const QString& path : paths) {
        QFile file(path);
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setCodec(QTextCodec::codecForName("UTF-8"));
            QString jsonStr = stream.readAll();
            file.close();
            
            cJSON* newRoot = cJSON_Parse(jsonStr.toUtf8().constData());
            if (newRoot) {
                if (m_root) {
                    cJSON_Delete((cJSON*)m_root);
                }
                m_root = newRoot;
                m_currentLang = langCode;
                emit languageChanged();
                return true;
            }
        }
    }
    
    return false;
}

QString Translator::t(const QString& key, const QStringList& args) const {
    if (!m_root) return key;
    
    QStringList parts = key.split('.');
    cJSON* current = (cJSON*)m_root;
    
    for (int i = 0; i < parts.size(); ++i) {
        current = cJSON_GetObjectItem(current, parts[i].toUtf8().constData());
        if (!current) return key; // Key not found
    }
    
    if (!cJSON_IsString(current)) return key;
    
    QString result = QString::fromUtf8(cJSON_GetStringValue(current));
    
    // Replace placeholders {0}, {1}, etc.
    for (int i = 0; i < args.size(); ++i) {
        result.replace(QString("{%1}").arg(i), args[i]);
    }
    
    return result;
}
