#include "compat34.h"
#include "mainwindow.h"
#include "translator.h"
#include "appsetup.h"
#include "logindialog.h"
#include "restapi.h"

#include "ThemeManager.h"
#include "LimeStyle.h"
#include "chatwidget.h"
#include "config.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

// 读取保存的语言设置（从 config.json）
static QString loadSavedLanguage() {
    return Config::value("uilang");
}


#include "app_icon.xpm"

int main(int argc, char* argv[]) {
	// os.setenv('QT_IM_MODULE', "xim", true)
    // os.setenv('XMODIFIERS', '@im=fcitx', true)
    setenv("QT_IM_MODULE", "xim", true);
    setenv("XMODIFIERS", "@im=fcitx", true);

    // Parse command line (before QApplication, for Qt3 compat)
    bool autoTranslateArg = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-auto-translate") == 0) {
            autoTranslateArg = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: qltox [options]\n"
                   "Options:\n"
                   "  -auto-translate    Automatically translate messages\n"
                   "  -h, --help         Show this help\n");
            return 0;
        }
    }
    ChatWidget::s_autoTranslateArg = autoTranslateArg;

    QApplication app(argc, argv);
#ifndef QT3_BUILD
    app.setApplicationVersion(APP_VERSION);
#endif

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
    qSetAppIcon(app_icon);
    qSetWindowTitle(&window, _("app_title"));
    window.show();
    qSetAppIcon(app_icon);  // show 后再设一次，触发 WM 重读 _NET_WM_ICON
    
    return app.exec();
}
