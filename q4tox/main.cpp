#include <QApplication>
#include <QTextCodec>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include "mainwindow.h"
#include "translator.h"

// Load saved language setting
static QString loadSavedLanguage() {
    QFile file(QDir::homePath() + "/.q4tox_lang");
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString lang = stream.readLine().trimmed();
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // Default: Simplified Chinese
}

// Save language setting
static void saveLanguage(const QString& lang) {
    QFile file(QDir::homePath() + "/.q4tox_lang");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream << lang << "\n";
        file.close();
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // Set UTF-8 codec
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    
    // Load saved language
    QString savedLang = loadSavedLanguage();
    
    // Load translation
    Translator::instance().loadLanguage(savedLang);
    
    // Create and show main window
    MainWindow window;
    window.show();
    
    return app.exec();
}
