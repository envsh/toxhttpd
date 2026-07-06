#include <QGuiApplication>
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>
#include <QScreen>
#include <QEvent>
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QskSkinManager.h>
#include <QskFontRole.h>
#include <QskWindow.h>
#include <QskLinearBox.h>
#include <QskStackBox.h>
#include <QskStackBoxAnimator.h>
#include <QskSkin.h>
#include <QskComboBox.h>
#include <QskSwitchButton.h>
#include "loginpage.h"
#include "mainpage.h"
#include "settingspage.h"
#include "aboutpage.h"

#include <memory>

struct FontSizes {
    int body, title, caption, global;
};

namespace {
class BackButtonFilter : public QObject
{
public:
    BackButtonFilter(QskStackBox* sb, MainPage* mp, QObject* parent = nullptr)
        : QObject(parent), m_stackBox(sb), m_mainPage(mp) {}

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Back) {
                event->accept();
                int idx = m_stackBox->currentIndex();
                if (idx == 2 || idx == 3) {
                    m_stackBox->setCurrentIndex(1);
                } else if (idx == 1) {
                    if (m_backTimer.isValid() && m_backTimer.elapsed() < 2000) {
                        QCoreApplication::quit();
                    } else {
                        m_backTimer.start();
                        m_mainPage->showToast("Press again to exit");
                    }
                } else {
                    QCoreApplication::quit();
                }
                return true;
            }
        }
        return false;
    }

private:
    QskStackBox* m_stackBox;
    MainPage* m_mainPage;
    QElapsedTimer m_backTimer;
};
} // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("fedlet");
    QCoreApplication::setApplicationName("qsktox");

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

    // Shared font sizes (updated by SettingsPage font scale combo)
    auto fontSizes = std::make_shared<FontSizes>(FontSizes{21, 29, 19, 16});

    // Font override lambda — re-applies on skin switch at runtime
    auto applyAndroidFonts = [fontSizes]() {
#ifdef Q_OS_ANDROID
        auto* s = qskSkinManager->skin();
        if (!s) {
            return;
        }
        auto makeFont = [](int pt) { QFont f; f.setPointSizeF(pt); return f; };
        s->setFont({QskFontRole::Body, QskFontRole::Normal},    makeFont(fontSizes->body));
        s->setFont({QskFontRole::Title, QskFontRole::Normal},   makeFont(fontSizes->title));
        s->setFont({QskFontRole::Caption, QskFontRole::Normal}, makeFont(fontSizes->caption));
        QGuiApplication::setFont(makeFont(fontSizes->global));
        qDebug() << "[qsktox] fonts re-applied (default family, CJK via font merging)";
#endif
    };
    applyAndroidFonts();
    QObject::connect(qskSkinManager, &QskSkinManager::skinChanged,
        &app, applyAndroidFonts);

    auto* rootBox = new QskLinearBox(Qt::Vertical);
    rootBox->setPanel(true);
    auto* stackBox = new QskStackBox(rootBox);
    stackBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    QskStackBoxAnimator* currentAnimator = new QskStackBoxAnimator4(stackBox);
    stackBox->setAnimator(currentAnimator);
    auto* loginPage = new LoginPage();
    auto* mainPage = new MainPage();

    auto* settingsPage = new SettingsPage();
    auto* aboutPage = new AboutPage();

    stackBox->addItem(loginPage);    // index 0
    stackBox->addItem(mainPage);     // index 1
    stackBox->addItem(settingsPage); // index 2
    stackBox->addItem(aboutPage);    // index 3

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

    // ── SettingsPage control connections ──
    QObject::connect(settingsPage->transitionCombo(),
        &QskComboBox::currentIndexChanged,
        [stackBox, &currentAnimator](int index) {
            QSettings().setValue("transition", index);
            qDebug() << "[qsktox] saved transition:" << index;
            QskStackBoxAnimator* newAnim = nullptr;
            switch (index) {
                case 0: newAnim = new QskStackBoxAnimator1(stackBox); break;
                case 1: newAnim = new QskStackBoxAnimator2(stackBox); break;
                case 2: newAnim = new QskStackBoxAnimator3(stackBox); break;
                case 3: newAnim = new QskStackBoxAnimator4(stackBox); break;
            }
            if (newAnim) {
                stackBox->setAnimator(newAnim);
                currentAnimator = newAnim;
            }
        });

    QObject::connect(settingsPage->skinCombo(),
        &QskComboBox::currentIndexChanged,
        [](int index) {
            QSettings().setValue("skin", index);
            qDebug() << "[qsktox] saved skin:" << index;
            static const char* names[] = {"Fusion", "Fluent2", "Material3"};
            if (index >= 0 && index < 3) {
                qskSkinManager->setSkin(names[index]);
            }
        });

    QObject::connect(settingsPage->darkModeSwitch(),
        &QskAbstractButton::toggled,
        [](bool checked) {
            QSettings().setValue("darkMode", checked);
            qDebug() << "[qsktox] saved darkMode:" << checked;
            auto* s = qskSkinManager->skin();
            if (s) {
                s->setColorScheme(checked
                    ? QskSkin::DarkScheme : QskSkin::LightScheme);
            }
        });

    QObject::connect(settingsPage->fontScaleCombo(),
        &QskComboBox::currentIndexChanged,
        [fontSizes, applyAndroidFonts](int index) {
            QSettings().setValue("fontScale", index);
            qDebug() << "[qsktox] saved fontScale:" << index;
            static const int sizes[][4] = {
                {16, 22, 14, 12},   // Small    (0.75x)
                {21, 29, 19, 16},   // Medium   (1.0x)
                {28, 39, 25, 21},   // Large    (1.33x)
                {35, 48, 32, 27},   // XL       (1.66x)
            };
            if (index >= 0 && index < 4) {
                fontSizes->body    = sizes[index][0];
                fontSizes->title   = sizes[index][1];
                fontSizes->caption = sizes[index][2];
                fontSizes->global  = sizes[index][3];
            }
            applyAndroidFonts();
        });

    // ── keepScreenOn 保存 ──
    QObject::connect(mainPage, &MainPage::keepScreenOnChanged,
        [](bool on) {
            QSettings().setValue("keepScreenOn", on);
            qDebug() << "[qsktox] saved keepScreenOn:" << on;
        });

    // ── 从持久化存储恢复设置 ──
    {
        QSettings settings;

        int v;
        v = settings.value("transition", 3).toInt();
        settingsPage->transitionCombo()->setCurrentIndex(v);
        qDebug() << "[qsktox] restored transition:" << v;

        v = settings.value("skin", 0).toInt();
        settingsPage->skinCombo()->setCurrentIndex(v);
        qDebug() << "[qsktox] restored skin:" << v;

        bool b = settings.value("darkMode", false).toBool();
        settingsPage->darkModeSwitch()->setChecked(b);
        qDebug() << "[qsktox] restored darkMode:" << b;

        v = settings.value("fontScale", 1).toInt();
        settingsPage->fontScaleCombo()->setCurrentIndex(v);
        qDebug() << "[qsktox] restored fontScale:" << v;

        b = settings.value("keepScreenOn", true).toBool();
        mainPage->setKeepScreenOn(b);
        qDebug() << "[qsktox] restored keepScreenOn:" << b
                 << "(JNI deferred to 50ms timer)";
    }

    // ── Android 生命周期 sync ──
    QObject::connect(&app, &QGuiApplication::applicationStateChanged,
        [](Qt::ApplicationState state) {
            if (state == Qt::ApplicationInactive
             || state == Qt::ApplicationSuspended) {
                qDebug() << "[qsktox] applicationState:" << state
                         << "-> syncing QSettings";
                QSettings().sync();
            }
        });

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

    app.installEventFilter(new BackButtonFilter(stackBox, mainPage));

    return app.exec();
}
