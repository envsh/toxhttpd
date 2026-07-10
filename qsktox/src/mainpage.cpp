#include "mainpage.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskTextField.h>
#include <QskPushButton.h>
#include <QskMenu.h>
#include <QskBoxShapeMetrics.h>
#include <QskLabelData.h>
#include <QTimer>
#include <QCoreApplication>
#include <QSettings>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

static constexpr int FLAG_KEEP_SCREEN_ON = 0x80;

static void jniKeepScreenOn(bool on) {
#ifdef Q_OS_ANDROID
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([on]() {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid()) return;
        QJniObject window = activity.callObjectMethod(
            "getWindow", "()Landroid/view/Window;");
        if (!window.isValid()) return;
        if (on)
            window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
        else
            window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    }).waitForFinished();
#else
    Q_UNUSED(on)
#endif
}

MainPage::MainPage(QQuickItem* parent)
    : Page(parent)
{
}

void MainPage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    QString url = launchArgs.value("url").toString();
    if (!url.isEmpty()) {
        qDebug() << "[MainPage] connecting to:" << url;
    }

    // Restore keepScreenOn, defer JNI to after window is ready
    m_keepScreenOn = QSettings().value("keepScreenOn", true).toBool();
    QTimer::singleShot(50, this, [this]() { jniKeepScreenOn(m_keepScreenOn); });

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── TopBar ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    topBar->setSpacing(8);

    // Left: Avatar (circular button with initial)
    auto* avatarBtn = new QskPushButton("U", topBar);
    avatarBtn->setPreferredSize(44, 44);
    avatarBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(100, Qt::RelativeSize));

    // Center: Title
    auto* title = new QskTextLabel("qsktox", topBar);
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    // Right: Options button
    auto* optionsBtn = new QskPushButton(QString::fromUtf8("⋮"), topBar);
    optionsBtn->setPreferredSize(44, 44);
    optionsBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));

    connect(optionsBtn, &QskAbstractButton::clicked, [this, optionsBtn]() {
        for (auto* old : findChildren<QskMenu*>())
            old->deleteLater();
        for (auto* old : findChildren<MenuOverlay*>())
            old->deleteLater();

        auto* menu = new QskMenu(this);
        menu->setModal(true);
        menu->addOption(QskLabelData(
            m_keepScreenOn ? QString::fromUtf8("✓ Keep Screen On")
                           : QString("  Keep Screen On")));
        menu->addSeparator();
        menu->addOption(QskLabelData("App Log"));
        menu->addSeparator();
        menu->addOption(QskLabelData("Settings"));
        menu->addSeparator();
        menu->addOption(QskLabelData("About"));
        menu->addSeparator();
        menu->addOption(QskLabelData("Logout"));

        QPointF btnPos = optionsBtn->mapToItem(this, QPointF(0, 0));
        qreal menuW = menu->implicitWidth();
        if (menuW <= 0) menuW = 200;
        menu->setOrigin(QPointF(
            btnPos.x() + optionsBtn->width() - menuW,
            btnPos.y() + optionsBtn->height()));

        connect(menu, &QskMenu::triggered, this, [this](int index) {
            if (index == 0) {
                m_keepScreenOn = !m_keepScreenOn;
                jniKeepScreenOn(m_keepScreenOn);
                emit keepScreenOnChanged(m_keepScreenOn);
            } else if (index == 2) {
                pageManager()->open("logs");
            } else if (index == 4) {
                pageManager()->open("settings");
            } else if (index == 6) {
                pageManager()->open("about");
            } else if (index == 8) {
                pageManager()->replace("login");
            }
            if (auto* m = qobject_cast<QskMenu*>(sender()))
                m->close();
        });

        {
            auto* overlay = new MenuOverlay(menu);
            connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
        }

        menu->open();
    });

    // ── ChatArea ──
    auto* chatArea = new QskLinearBox(Qt::Vertical, layout);
    chatArea->setPanel(true);
    chatArea->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Expanding);

    chatArea->addStretch(1);
    auto* welcome = new QskTextLabel("Welcome to qsktox", chatArea);
    welcome->setAlignment(Qt::AlignCenter);
    chatArea->addStretch(1);

    // ── Toast label (initially hidden) ──
    m_toastLabel = new QskTextLabel("", chatArea);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->setVisible(false);
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() {
        m_toastLabel->setVisible(false);
    });

    // ── InputBar ──
    auto* inputBar = new QskLinearBox(Qt::Horizontal, layout);
    inputBar->setPanel(true);
    inputBar->setPreferredHeight(56);

    inputBar->addSpacer(8, 0);
    auto* input = new QskTextField(inputBar);
    input->setPlaceholderText("Type a message");
    input->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    input->setPreferredHeight(40);

    auto* sendBtn = new QskPushButton(QString::fromUtf8("→"), inputBar);
    sendBtn->setPreferredWidth(48);
    sendBtn->setPreferredHeight(40);
    inputBar->addSpacer(8, 0);
}

void MainPage::onNewIntent(const QVariantMap& launchArgs)
{
    QString url = launchArgs.value("url").toString();
    if (!url.isEmpty()) {
        qDebug() << "[MainPage] onNewIntent, url:" << url;
    }
}

void MainPage::showToast(const QString& msg, int durationMs) {
    m_toastLabel->setText(msg);
    m_toastLabel->setVisible(true);
    m_toastTimer->start(durationMs);
}

void MainPage::setKeepScreenOn(bool on) {
    if (m_keepScreenOn == on) return;
    m_keepScreenOn = on;
}
