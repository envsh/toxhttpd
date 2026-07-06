#include <QGuiApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <QScreen>
#include <QskSkinManager.h>
#include <QskLinearBox.h>
#include <QskWindow.h>
#include <QskBox.h>
#include <QskTextLabel.h>

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

    auto* box = new QskLinearBox(Qt::Vertical);
    box->setPanel(true);
    auto* label = new QskTextLabel("Hello qsktox", box);
    label->setAlignment(Qt::AlignCenter);

    QskWindow window;
    window.addItem(box);
    window.setAutoLayoutChildren(true);

#ifdef Q_OS_ANDROID
    window.showFullScreen();
    qDebug() << "[qsktox] window size:" << window.size()
             << "contentItem size:" << window.contentItem()->size()
             << "isExposed:" << window.isExposed();
    window.update();
#else
    window.setPreferredSize({400, 300});
    window.show();
#endif

    return app.exec();
}
