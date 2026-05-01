#include <qapplication.h>
#include <qtextcodec.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qdir.h>
#include "mainwindow.h"
#include "translator.h"

#include "ThemeManager.h"

// 读取保存的语言设置（替代 QSettings）
static QString loadSavedLanguage() {
    QFile file(QDir::homeDirPath() + "/.q3tox_lang");
    if (file.exists() && file.open(IO_ReadOnly)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString lang = stream.readLine().stripWhiteSpace();
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // 默认简体
}

// 保存语言设置
static void saveLanguage(const QString& lang) {
    QFile file(QDir::homeDirPath() + "/.q3tox_lang");
    if (file.open(IO_WriteOnly)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream << lang << "\n";
        file.close();
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    
    ThemeManager::applyTheme(true);
    
    // 读取保存的语言设置
    QString savedLang = loadSavedLanguage();
    
    // 加载语言
    Translator::instance().loadLanguage(savedLang);
    
    // 创建主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}
