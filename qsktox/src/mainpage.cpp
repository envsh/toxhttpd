#include "mainpage.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskTextField.h>
#include <QskPushButton.h>
#include <QskMenu.h>
#include <QskBoxShapeMetrics.h>
#include <QskLabelData.h>
#include <QTimer>
#include <QJniObject>

static constexpr int FLAG_KEEP_SCREEN_ON = 0x80;

static void setKeepScreenOn(bool on) {
#ifdef Q_OS_ANDROID
    auto activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity", "()Landroid/app/Activity;");
    if (!activity.isValid()) return;
    auto window = activity.callObjectMethod(
        "getWindow", "()Landroid/view/Window;");
    if (!window.isValid()) return;
    if (on)
        window.callMethod<void>("addFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
    else
        window.callMethod<void>("clearFlags", "(I)V", FLAG_KEEP_SCREEN_ON);
#else
    Q_UNUSED(on)
#endif
}

MainPage::MainPage(QQuickItem* parent)
    : QskControl(parent)
{
    setKeepScreenOn(true);
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
    auto* optionsBtn = new QskPushButton(QString::fromUtf8("\u22EE"), topBar);
    optionsBtn->setPreferredSize(44, 44);
    optionsBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));

    connect(optionsBtn, &QskAbstractButton::clicked, [this, optionsBtn]() {
        // 清理旧菜单（避免残留动画状态导致崩溃）
        for (auto* old : findChildren<QskMenu*>())
            old->deleteLater();

        auto* menu = new QskMenu(this);
        menu->setPopupFlag(QskPopup::CloseOnPressOutside, true);
        menu->addOption(QskLabelData(
            m_keepScreenOn ? QString::fromUtf8("\u2713 Keep Screen On")
                           : QString("  Keep Screen On")));
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
                setKeepScreenOn(m_keepScreenOn);
            } else if (index == 2) {
                emit settingsRequested();
            } else if (index == 4) {
                emit aboutRequested();
            } else if (index == 6) {
                emit logoutRequested();
            }
        });

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

    auto* sendBtn = new QskPushButton(QString::fromUtf8("\u2192"), inputBar);
    sendBtn->setPreferredWidth(48);
    sendBtn->setPreferredHeight(40);
    inputBar->addSpacer(8, 0);
}

void MainPage::showToast(const QString& msg, int durationMs) {
    m_toastLabel->setText(msg);
    m_toastLabel->setVisible(true);
    m_toastTimer->start(durationMs);
}
