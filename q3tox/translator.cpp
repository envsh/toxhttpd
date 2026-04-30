#include "translator.h"
#include "cJSON.h"
#include <qfile.h>
#include <qtextstream.h>
#include <qapplication.h>
#include <qtextcodec.h>

Translator& Translator::instance() {
    static Translator instance;
    return instance;
}

Translator::Translator() : m_root(nullptr), m_currentLang("zh-CN") {
    // 默认加载简体中文
    loadLanguage("zh-CN");
}

Translator::~Translator() {
    if (m_root) cJSON_Delete((cJSON*)m_root);
}

bool Translator::loadLanguage(const QString& langCode) {
    // 加载 lang 目录下的 JSON 文件
    // Qt3 没有 applicationDirPath，使用当前目录
    QString filepath = "./lang/" + langCode + ".json";
    QFile file(filepath);
    if (!file.exists()) {
        qWarning("Language file not found: %s", (const char*)filepath.local8Bit());
        return false;
    }
    
    if (!file.open(IO_ReadOnly)) {
        qWarning("Cannot open language file: %s", (const char*)filepath.local8Bit());
        return false;
    }
    
    QTextStream stream(&file);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    QString jsonStr = stream.read();
    file.close();
    
    // 解析 JSON
    cJSON* newRoot = cJSON_Parse(jsonStr.utf8());
    if (!newRoot) {
        qWarning("Failed to parse JSON: %s", (const char*)filepath.local8Bit());
        return false;
    }
    
    // 替换旧数据
    if (m_root) cJSON_Delete((cJSON*)m_root);
    m_root = newRoot;
    m_currentLang = langCode;
    
    emit languageChanged();
    qWarning("Language loaded: %s", (const char*)langCode.local8Bit());
    return true;
}

QString Translator::t(const QString& key, const QStringList& args) const {
    if (!m_root) return key;
    
    // 解析点号分隔的键
    QStringList parts = QStringList::split('.', key);
    cJSON* current = (cJSON*)m_root;
    
    for (int i = 0; i < (int)parts.size(); ++i) {
        current = cJSON_GetObjectItem(current, parts[i].utf8());
        if (!current) {
            qWarning("Translation missing: %s", (const char*)key.local8Bit());
            return key;
        }
    }
    
    if (!cJSON_IsString(current)) {
        qWarning("Translation is not a string: %s", (const char*)key.local8Bit());
        return key;
    }
    
    QString result = QString::fromUtf8(cJSON_GetStringValue(current));
    
    // 替换占位符 {0}, {1}...
    for (int i = 0; i < (int)args.size(); ++i) {
        result.replace(QString("{%1}").arg(i), args[i]);
    }
    
    return result;
}
