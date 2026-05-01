#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include "cJSON.h"

class Translator : public QObject {
    Q_OBJECT
public:
    static Translator& instance();
    
    bool loadLanguage(const QString& langCode);
    QString t(const QString& key, const QStringList& args = QStringList()) const;
    
    QString currentLanguage() const { return m_currentLang; }
    
signals:
    void languageChanged();
    
private:
    Translator(QObject* parent = 0);
    ~Translator();
    
    QString getExeDir() const;
    cJSON* m_root;
    QString m_currentLang;
};

// Convenience macros
#define _(key) Translator::instance().t(key)
#define _A(key, args) Translator::instance().t(key, args)

#endif // TRANSLATOR_H
