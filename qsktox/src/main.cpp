#include <QGuiApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <QScreen>
#include <QskSkinManager.h>
#include <QskFontRole.h>
#include <QskWindow.h>
#include <QskLinearBox.h>
#include <QskStackBox.h>
#include "loginpage.h"
#include "mainpage.h"
#include "settingspage.h"
#include "aboutpage.h"
#include <QskStackBoxAnimator.h>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

#ifdef Q_OS_ANDROID
    qputenv("QSG_RENDER_LOOP", "basic");
#endif

#ifdef Q_OS_ANDROID
    const auto dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    qDebug() << "[qsktox] AppLocalDataLocation:" << dataDir;

    const auto libPaths = QCoreApplication::libraryPaths();
    qDebug() << "[qsktox] libraryPaths:" << libPaths;

    for (const auto& libPath : libPaths) {
        QDir srcDir(libPath);
        qDebug() << "[qsktox] checking path:" << libPath
                 << "exists=" << srcDir.exists()
                 << "has_qskinny=" << srcDir.exists("libqskinny_arm64-v8a.so");

        if (!srcDir.exists("libqskinny_arm64-v8a.so"))
            continue;

        QDir().mkpath(dataDir + "/skins");
        for (const auto& name : {"libfusionskin", "libfluent2skin",
                                  "libmaterial3skin"}) {
            const QString fileName = name + QString("_arm64-v8a.so");
            const QString dst = dataDir + "/skins/" + fileName;

            if (QFile::exists(dst)) {
                qDebug() << "[qsktox] already exists, skip:" << dst;
                continue;
            }

            const QString src = srcDir.absoluteFilePath(fileName);
            qDebug() << "[qsktox] copying" << src << "->" << dst;

            if (QFile::copy(src, dst)) {
                QFile(dst).setPermissions(
                    QFile::ReadOwner | QFile::ExeOwner |
                    QFile::ReadUser  | QFile::ExeUser);
                qDebug() << "[qsktox] copy OK";
            } else {
                qWarning() << "[qsktox] copy FAILED:" << src;
            }
        }
        break;
    }
    QCoreApplication::addLibraryPath(dataDir);
    qDebug() << "[qsktox] added libraryPath:" << dataDir;
#endif

    qDebug() << "[qsktox] calling setPluginPaths...";
    qskSkinManager->setPluginPaths(QCoreApplication::libraryPaths());

    const auto availableSkins = qskSkinManager->skinNames();
    qDebug() << "[qsktox] available skins:" << availableSkins;

    qskSkinManager->setSkin("Fusion");
    qDebug() << "[qsktox] skin OK:" << qskSkinManager->skinName();
    qDebug() << "[qsktox] DPI:" << QGuiApplication::primaryScreen()->logicalDotsPerInch()
             << "dpr:" << QGuiApplication::primaryScreen()->devicePixelRatio();

#ifdef Q_OS_ANDROID
    // Override skin fonts to use pointSize (DPI-aware) instead of Fusion's pixelSize
    auto* skin = qskSkinManager->skin();
    if (skin) {
        QFont body("sans-serif", 16);
        skin->setFont({QskFontRole::Body, QskFontRole::Normal}, body);

        QFont title("sans-serif", 22);
        skin->setFont({QskFontRole::Title, QskFontRole::Normal}, title);
        skin->setFont({QskFontRole::Caption, QskFontRole::Normal}, QFont("sans-serif", 14));
        qDebug() << "[qsktox] fonts overridden for Android DPI";
    }
#endif

    auto* rootBox = new QskLinearBox(Qt::Vertical);
    rootBox->setPanel(true);
    auto* stackBox = new QskStackBox(rootBox);
    stackBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    auto* loginPage = new LoginPage();
    auto* mainPage = new MainPage();

    auto* settingsPage = new SettingsPage();
    auto* aboutPage = new AboutPage();

    stackBox->addItem(loginPage);    // index 0
    stackBox->addItem(mainPage);     // index 1
    stackBox->addItem(settingsPage); // index 2
    stackBox->addItem(aboutPage);    // index 3

    stackBox->setAnimator(new QskStackBoxAnimator3(stackBox));
    stackBox->setCurrentIndex(0);

    QObject::connect(loginPage, &LoginPage::accepted,
        [stackBox](const QString& url) {
            qDebug() << "[qsktox] connecting to:" << url;
            stackBox->setCurrentIndex(1);
        });

    QObject::connect(mainPage, &MainPage::settingsRequested,
        [stackBox]() { stackBox->setCurrentIndex(2); });
    QObject::connect(mainPage, &MainPage::aboutRequested,
        [stackBox]() { stackBox->setCurrentIndex(3); });
    QObject::connect(mainPage, &MainPage::logoutRequested,
        [stackBox]() { stackBox->setCurrentIndex(0); });
    QObject::connect(settingsPage, &SettingsPage::backRequested,
        [stackBox]() { stackBox->setCurrentIndex(1); });
    QObject::connect(aboutPage, &AboutPage::backRequested,
        [stackBox]() { stackBox->setCurrentIndex(1); });

    QskWindow window;
    window.addItem(rootBox);

#ifdef Q_OS_ANDROID
    window.show();
    qDebug() << "[qsktox] window size:" << window.size()
             << "contentItem size:" << window.contentItem()->size()
             << "isExposed:" << window.isExposed();
    window.update();
#else
    window.setPreferredSize({420, 780});
    window.show();
#endif

    return app.exec();
}
