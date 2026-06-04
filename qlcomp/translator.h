#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include "compat34.h"
#include <qstring.h>
#include <qstringlist.h>
#include <qobject.h>

class Translator : public QObject {
    Q_OBJECT
public:
    static Translator& instance();
    void addTranslationPath(const QString& path);
    bool loadLanguage(const QString& langCode); // "zh-CN", "zh-TW", "en-US"
    QString t(const QString& key, const QStringList& args = QStringList()) const;
    QString currentLang() const { return m_currentLang; }
    
signals:
    void languageChanged();
    
private:
    Translator();
    ~Translator();
    void loadJsonFile(const QString& filepath);
    QString getNestedValue(void* root, const QString& key) const;
    
    QString m_currentLang;
    void* m_root; // cJSON root object
    QStringList m_searchPaths;
};

// 便捷宏：避免与 QObject::tr() 冲突
#define _(key) Translator::instance().t(key)
#define _A(key, args) Translator::instance().t(key, args)

#endif // TRANSLATOR_H
