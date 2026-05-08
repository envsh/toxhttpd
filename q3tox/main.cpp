#include "compat34.h"
#include "mainwindow.h"
#include "translator.h"
#include "appsetup.h"

#include "ThemeManager.h"

// 读取保存的语言设置（替代 QSettings）
static QString loadSavedLanguage() {
    QString home = qGetHomePath();
    QFile file(home + "/.q3tox_lang");
    if (file.exists() && qOpenReadOnly(file)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        QString lang = qTrim(stream.readLine());
        file.close();
        if (!lang.isEmpty()) return lang;
    }
    return "zh-CN"; // 默认简体
}

// 保存语言设置
static void saveLanguage(const QString& lang) {
    QString home = qGetHomePath();
    QFile file(home + "/.q3tox_lang");
    if (qOpenWriteOnly(file)) {
        QTextStream stream(&file);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream << lang << "\n";
        file.close();
    }
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QtappSetup::setup(app);
    
    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
//    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    
    ThemeManager::applyTheme(true);
    
    // 读取保存的语言设置
    QString savedLang = loadSavedLanguage();
    
    // 加载语言
    Translator::instance().loadLanguage(savedLang);
    
    // 创建主窗口
    MainWindow window;
    qSetWindowTitle(&window, _("app_title"));
    window.show();
    
    return app.exec();
}
