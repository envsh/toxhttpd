#include "../ConfigDialog.h"
#include "../ThemeManager.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#else
#include <QApplication>
#endif

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    bool darkMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--theme" && i + 1 < argc) {
            if (QString(argv[i + 1]) == "dark") {
                darkMode = true;
            }
        }
    }
    
    if (darkMode) {
        ThemeManager::applyTheme(true);
    }
    
    ConfigDialog dialog(QString::fromUtf8("设置演示"));
    dialog.setSettingsFile(QString::fromUtf8("./config.ini"));
    
    CategoryPage* page1 = new CategoryPage(QString::fromUtf8("常规"), &dialog);
    BoolConfigItem* enableOpt = new BoolConfigItem("enableFeature", true, QString::fromUtf8("启用功能"), page1);
    StringConfigItem* nameOpt = new StringConfigItem("userName", "", QString::fromUtf8("用户名"), page1);
    StringConfigItem* pathOpt = new StringConfigItem("savePath", "/tmp", QString::fromUtf8("保存路径"), page1);
    IntConfigItem* portOpt = new IntConfigItem("port", 8080, QString::fromUtf8("端口号"), 1, 65535, page1);
    StringConfigItem* serverOpt = new StringConfigItem("server", "server.example.com", QString::fromUtf8("服务器"), page1);
    StringConfigItem* emailOpt = new StringConfigItem("email", "admin@example.com", QString::fromUtf8("邮箱"), page1);
    IntConfigItem* maxOpt = new IntConfigItem("maxConnections", 100, QString::fromUtf8("最大连接数"), 1, 1000, page1);
    BoolConfigItem* sslOpt = new BoolConfigItem("enableSSL", true, QString::fromUtf8("启用SSL"), page1);
    StringConfigItem* descOpt = new StringConfigItem("description", "", QString::fromUtf8("描述"), page1);
    IntConfigItem* retryOpt = new IntConfigItem("retryCount", 3, QString::fromUtf8("重试次数"), 0, 10, page1);
    IntConfigItem* timeoutOpt = new IntConfigItem("connTimeout", 30, QString::fromUtf8("连接超时"), 5, 300, page1);
    dialog.registerConfigItem(enableOpt);
    dialog.registerConfigItem(nameOpt);
    dialog.registerConfigItem(pathOpt);
    dialog.registerConfigItem(portOpt);
    dialog.registerConfigItem(serverOpt);
    dialog.registerConfigItem(emailOpt);
    dialog.registerConfigItem(maxOpt);
    dialog.registerConfigItem(sslOpt);
    dialog.registerConfigItem(descOpt);
    dialog.registerConfigItem(retryOpt);
    dialog.registerConfigItem(timeoutOpt);
    
    page1->addWidget(enableOpt->checkBox());
    page1->addLabeledControl(QString::fromUtf8("用户名:"), nameOpt->lineEdit());
    page1->addLabeledControl(QString::fromUtf8("保存路径:"), pathOpt->lineEdit());
    page1->addLabeledControl(QString::fromUtf8("端口号:"), portOpt->spinBox());
    page1->addLabeledControl(QString::fromUtf8("服务器:"), serverOpt->lineEdit());
    page1->addLabeledControl(QString::fromUtf8("邮箱:"), emailOpt->lineEdit());
    page1->addLabeledControl(QString::fromUtf8("最大连接数:"), maxOpt->spinBox());
    page1->addWidget(sslOpt->checkBox());
    page1->addLabeledControl(QString::fromUtf8("描述:"), descOpt->lineEdit());
    page1->addLabeledControl(QString::fromUtf8("重试次数:"), retryOpt->spinBox());
    page1->addLabeledControl(QString::fromUtf8("连接超时:"), timeoutOpt->spinBox());
    page1->addStretch(0);
    
    CategoryPage* page2 = new CategoryPage(QString::fromUtf8("高级"), &dialog);
    IntConfigItem* timeoutOpt2 = new IntConfigItem("timeout", 30, QString::fromUtf8("超时(秒)"), 1, 300, page2);
    QStringList options;
    options << QString::fromUtf8("选项 A") << QString::fromUtf8("选项 B") << QString::fromUtf8("选项 C");
    SelectConfigItem* modeOpt = new SelectConfigItem("mode", QString::fromUtf8("选项 A"), QString::fromUtf8("模式"), options, page2);
    BoolConfigItem* debugOpt = new BoolConfigItem("debug", false, QString::fromUtf8("调试模式"), page2);
    StringConfigItem* hostOpt = new StringConfigItem("host", "localhost", QString::fromUtf8("主机地址"), page2);
    dialog.registerConfigItem(timeoutOpt2);
    dialog.registerConfigItem(modeOpt);
    dialog.registerConfigItem(debugOpt);
    dialog.registerConfigItem(hostOpt);
    
    page2->addLabeledControl(QString::fromUtf8("超时:"), timeoutOpt2->spinBox());
    page2->addLabeledControl(QString::fromUtf8("模式:"), modeOpt->comboBox());
    page2->addLabeledControl(QString::fromUtf8("调试:"), debugOpt->checkBox());
    page2->addLabeledControl(QString::fromUtf8("主机:"), hostOpt->lineEdit());
    page2->addStretch(0);
    
    dialog.addCategory(QString::fromUtf8("常规"), page1);
    dialog.addCategory(QString::fromUtf8("高级"), page2);
    
    dialog.setCurrentCategory(0);
    dialog.loadSettings();
    
    if (dialog.exec() == QDialog::Accepted) {
        dialog.saveSettings();
    }
    
    return 0;
}
