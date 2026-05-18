#include "compat34.h"
#include "mainwindow.h"
#include "translator.h"
#include "appsetup.h"
#include "logindialog.h"
#include "restapi.h"

#include "ThemeManager.h"
#include "LimeStyle.h"

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
	// os.setenv('QT_IM_MODULE', "xim", true)
    // os.setenv('XMODIFIERS', '@im=fcitx', true)
    setenv("QT_IM_MODULE", "xim", true);
    setenv("XMODIFIERS", "@im=fcitx", true);

    QApplication app(argc, argv);

    QtappSetup::setup(app);
        
    app.setStyle(new LimeStyle);
    ThemeManager::setStyle("qtFusion", true);
    
    // 读取保存的语言设置
    QString savedLang = loadSavedLanguage();
    
    // 加载语言
    Translator::instance().loadLanguage(savedLang);
    QtappSetup::installQtTranslations(savedLang);
    
    // 登录对话框 — 临时阻止 accept() → hide() 触发 exit(0)
    QtappSetup::setQuitOnExit(false);

    LoginDialog loginDialog;
    if (loginDialog.exec() != QDialog::Accepted) {
        return 0;
    }

    // 对话框接受后恢复，MainWindow关闭时仍能正常退出
    QtappSetup::setQuitOnExit(true);

    ToxAPI::setBaseUrl(loginDialog.selectedUrl());
    
    // 创建主窗口
    MainWindow window;
    qSetWindowTitle(&window, _("app_title"));
    window.show();
    
    return app.exec();
}
