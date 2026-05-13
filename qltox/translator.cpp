#include "translator.h"
#include "cJSON.h"
#include "compat34.h"
#include <unistd.h>

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
    // 尝试多个路径加载 lang 目录下的 JSON 文件
    QStringList paths;
    
    // 1. 使用当前可执行文件的路径
    char exePath[1024];
    int len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        QString exeStr(exePath);
        int lastSlash = qLastIndexOf(exeStr, "/");
        if (lastSlash >= 0) {
            paths.append(exeStr.left(lastSlash + 1) + "lang/" + langCode + ".json");
        }
    }
    
    // 2. 当前工作目录
    paths.append("lang/" + langCode + ".json");
    
    // 3. 绝对路径（可执行文件所在目录的上级目录）
    paths.append("/home/gzleo/aprog/toxhttpd/q3tox/lang/" + langCode + ".json");
    
    QString filepath;
    for (const QString& p : paths) {
        QFile file(p);
        if (file.exists()) {
            filepath = p;
            break;
        }
    }
    
    if (filepath.isEmpty()) {
        qWarning("Language file not found for: %s (tried %d paths)", 
                 qToUtf8(langCode).data(), paths.size());
        for (int i = 0; i < (int)paths.size(); ++i) {
            qWarning("  Path %d: %s", i, qToUtf8(paths[i]).data());
        }
        return false;
    }
    
    QFile file(filepath);
    file.close(); // 确保文件处于关闭状态
    if (!qOpenReadOnly(file)) {
        qWarning("Cannot open language file: %s", qToUtf8(filepath).data());
        return false;
    }
    
    QTextStream stream(&file);
    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    QString jsonStr;
    while (!stream.atEnd()) {
        jsonStr += stream.readLine();
    }
    file.close(); // 显式关闭文件
    qWarning("Language file loaded: %s", qToUtf8(filepath).data());
    
    // 解析 JSON
    cJSON* newRoot = cJSON_Parse(qToUtf8(jsonStr).data());
    if (!newRoot) {
        qWarning("Failed to parse JSON: %s", qToUtf8(filepath).data());
        return false;
    }
    
    // 调试：打印加载信息
    qWarning("DEBUG loadLanguage: file=%s, newRoot=%p", qToUtf8(filepath).data(), newRoot);
    
    // 替换旧数据
    if (m_root) cJSON_Delete((cJSON*)m_root);
    m_root = newRoot;
    m_currentLang = langCode;
    
    emit languageChanged();
    qWarning("Language loaded: %s", qToUtf8(langCode).data());
    return true;
}

QString Translator::t(const QString& key, const QStringList& args) const {
    if (!m_root) {
        qWarning("Translator: m_root is nullptr, returning key: %s", qToUtf8(key).data());
        return key;
    }
    
    // 解析点号分隔的键
    QStringList parts = qSplit(key, ".");
    cJSON* current = (cJSON*)m_root;
    
    // 调试输出
    qWarning("DEBUG t: key=%s, parts.size=%d, m_root=%p", qToUtf8(key).data(), parts.size(), m_root);
    
    for (int i = 0; i < (int)parts.size(); ++i) {
        qWarning("DEBUG t: i=%d, part=%s, current=%p, type=%d", i, qToUtf8(parts[i]).data(), current, ((cJSON*)current)->type);
        current = cJSON_GetObjectItem(current, qToUtf8(parts[i]).data());
        qWarning("DEBUG t: after cJSON_GetObjectItem, current=%p, type=%d", current, current ? ((cJSON*)current)->type : -1);
        if (!current) {
            qWarning("Translation missing: %s (failed at part %d: %s)", 
                     qToUtf8(key).data(), i, qToUtf8(parts[i]).data());
            // 调试：打印父对象的所有键
            if (i > 0) {
                cJSON* parent = (cJSON*)m_root;
                for (int j = 0; j < i; ++j) {
                    parent = cJSON_GetObjectItem(parent, qToUtf8(parts[j]).data());
                }
                if (parent) {
                    qWarning("DEBUG t: parent object keys:");
                    // cJSON 没有直接遍历对象键的 API，但我们可以打印整个对象
                    char* parentStr = cJSON_Print(parent);
                    qWarning("DEBUG t: parent=%s", parentStr);
                    free(parentStr);
                }
            }
            return key;
        }
    }
    
    if (!cJSON_IsString(current)) {
        qWarning("Translation is not a string: %s", qToUtf8(key).data());
        return key;
    }
    
    QString result = QString::fromUtf8(cJSON_GetStringValue(current));
    
    // 替换占位符 {0}, {1}...
    for (int i = 0; i < (int)args.size(); ++i) {
        result.replace(QString("{%1}").arg(i), args[i]);
    }
    
    return result;
}
