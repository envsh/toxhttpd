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
#include "logpage.h"
#include "pagemanager.h"

#include <memory>

namespace {

class BackButtonFilter : public QObject
{
public:
    BackButtonFilter(PageManager* pm, QObject* parent = nullptr)
        : QObject(parent), m_pageManager(pm) {}

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Back) {
                event->accept();
                if (m_pageManager->depth() > 1) {
                    m_pageManager->back();
                } else {
                    QString id = m_pageManager->currentPageId();
                    if (id == "main") {
                        if (m_backTimer.isValid() && m_backTimer.elapsed() < 2000) {
                            QCoreApplication::quit();
                        } else {
                            m_backTimer.start();
                            auto* mainPage = qobject_cast<MainPage*>(
                                m_pageManager->findPage("main"));
                            if (mainPage) {
                                mainPage->showToast("Press again to exit");
                            }
                        }
                    } else {
                        QCoreApplication::quit();
                    }
                }
                return true;
            }
        }
        return false;
    }

private:
    PageManager* m_pageManager;
    QElapsedTimer m_backTimer;
};

void applyAndroidFonts(const std::shared_ptr<FontSizes>& fontSizes)
{
#ifdef Q_OS_ANDROID
    auto* s = qskSkinManager->skin();
    if (!s) return;
    auto makeFont = [](int pt) { QFont f; f.setPointSizeF(pt); return f; };
    s->setFont({QskFontRole::Body, QskFontRole::Normal},    makeFont(fontSizes->body));
    s->setFont({QskFontRole::Title, QskFontRole::Normal},   makeFont(fontSizes->title));
    s->setFont({QskFontRole::Caption, QskFontRole::Normal}, makeFont(fontSizes->caption));
    QGuiApplication::setFont(makeFont(fontSizes->global));
    qDebug() << "[qsktox] fonts re-applied (default family, CJK via font merging)";
#else
    Q_UNUSED(fontSizes)
#endif
}

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

    // ── Shared font state ──
    auto fontSizes = std::make_shared<FontSizes>();
    fontSizes->body    = 21;
    fontSizes->title   = 29;
    fontSizes->caption = 19;
    fontSizes->global  = 16;

    auto fontApplier = [fontSizes]() { applyAndroidFonts(fontSizes); };
    fontApplier();

    SettingsPage::sharedFontSizes = fontSizes;
    SettingsPage::applyAndroidFonts = fontApplier;

    QObject::connect(qskSkinManager, &QskSkinManager::skinChanged,
        &app, fontApplier);

    // ── Root layout ──
    auto* rootBox = new QskLinearBox(Qt::Vertical);
    rootBox->setPanel(true);
    auto* stackBox = new QskStackBox(rootBox);
    stackBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    auto* defaultAnimator = new QskStackBoxAnimator4(stackBox);
    stackBox->setAnimator(defaultAnimator);

    // ── PageManager ──
    auto* pageManager = new PageManager(stackBox);

    // ── Restore persisted settings and apply to global state ──
    {
        QSettings s;

        int skinIdx = s.value("skin", 0).toInt();
        if (skinIdx > 0) {
            static const char* names[] = {"Fusion", "Fluent2", "Material3"};
            if (skinIdx < 3) {
                qskSkinManager->setSkin(names[skinIdx]);
            }
        }
        bool dark = s.value("darkMode", false).toBool();
        if (dark) {
            auto* skin = qskSkinManager->skin();
            if (skin) skin->setColorScheme(QskSkin::DarkScheme);
        }
        int fontIdx = s.value("fontScale", 1).toInt();
        if (fontIdx >= 0 && fontIdx < 4) {
            static const int sizes[][4] = {
                {16, 22, 14, 12},
                {21, 29, 19, 16},
                {28, 39, 25, 21},
                {35, 48, 32, 27},
            };
            fontSizes->body    = sizes[fontIdx][0];
            fontSizes->title   = sizes[fontIdx][1];
            fontSizes->caption = sizes[fontIdx][2];
            fontSizes->global  = sizes[fontIdx][3];
            fontApplier();
        }
        int transIdx = s.value("transition", 3).toInt();
        if (transIdx != 3) {
            QskStackBoxAnimator* a = nullptr;
            switch (transIdx) {
                case 0: a = new QskStackBoxAnimator1(stackBox); break;
                case 1: a = new QskStackBoxAnimator2(stackBox); break;
                case 2: a = new QskStackBoxAnimator3(stackBox); break;
            }
            if (a) stackBox->setAnimator(a);
        }
    }

    // ── Register pages ──
    pageManager->registerPage("login", []() -> Page* {
        return new LoginPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("main", [&]() -> Page* {
        auto* page = new MainPage();
        QObject::connect(page, &MainPage::keepScreenOnChanged,
            [](bool on) {
                QSettings().setValue("keepScreenOn", on);
                qDebug() << "[qsktox] saved keepScreenOn:" << on;
            });
        return page;
    }, {CachePolicy::Permanent, LaunchMode::Standard});

    pageManager->registerPage("settings", []() -> Page* {
        return new SettingsPage();
    }, {CachePolicy::Permanent, LaunchMode::Standard});

    pageManager->registerPage("about", []() -> Page* {
        return new AboutPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("logs", []() -> Page* {
        return new LogPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    // ── Start with login page ──
    pageManager->open("login");

    // ── Application lifecycle → sync QSettings ──
    QObject::connect(&app, &QGuiApplication::applicationStateChanged,
        [](Qt::ApplicationState state) {
            if (state == Qt::ApplicationInactive
             || state == Qt::ApplicationSuspended) {
                qDebug() << "[qsktox] applicationState:" << state
                         << "-> syncing QSettings";
                QSettings().sync();
            }
        });

    // ── Window ──
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

    app.installEventFilter(new BackButtonFilter(pageManager));

    return app.exec();
}
