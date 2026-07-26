#include "settingspage.h"
#include "pagemanager.h"
#include "phonemonitor.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskComboBox.h>
#include <QskSwitchButton.h>
#include <QskLabelData.h>
#include <QskSeparator.h>
#include <QskStackBox.h>
#include <QskStackBoxAnimator.h>
#include <QskSetup.h>
#include <QskItem.h>
#include <QskSkinManager.h>
#include <QskSkin.h>
#include <QSettings>
#include <QDebug>
#if defined(Q_OS_ANDROID)
#include <QJniObject>
#endif

std::shared_ptr<FontSizes> SettingsPage::sharedFontSizes;
std::function<void()>      SettingsPage::applyAndroidFonts;

QPointer<SettingsPage> SettingsPage::s_instance;

SettingsPage::SettingsPage(QQuickItem* parent)
    : Page(parent)
    , m_debugBgSwitch(nullptr)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── TopBar ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    auto* title = new QskTextLabel("Settings", topBar);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignCenter);

    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        finish();
    });

    layout->addSpacer(24, 0);

    // ── Row 1: Page Transition ──
    auto* row1 = new QskLinearBox(Qt::Horizontal, layout);
    row1->setPreferredHeight(48);
    row1->setSpacing(12);
    auto* transitionLabel = new QskTextLabel("Page Transition", row1);
    transitionLabel->setPreferredWidth(160);
    m_transitionCombo = new QskComboBox(row1);
    m_transitionCombo->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_transitionCombo->addOption(QskLabelData("Slide"));
    m_transitionCombo->addOption(QskLabelData("2D"));
    m_transitionCombo->addOption(QskLabelData("3D"));
    m_transitionCombo->addOption(QskLabelData("Perspective"));
    m_transitionCombo->setCurrentIndex(3);

    new QskSeparator(Qt::Horizontal, layout);

    // ── Row 2: Theme ──
    auto* row2 = new QskLinearBox(Qt::Horizontal, layout);
    row2->setPreferredHeight(48);
    row2->setSpacing(12);
    auto* themeLabel = new QskTextLabel("Theme", row2);
    themeLabel->setPreferredWidth(160);
    m_skinCombo = new QskComboBox(row2);
    m_skinCombo->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_skinCombo->addOption(QskLabelData("Fusion"));
    m_skinCombo->addOption(QskLabelData("Fluent2"));
    m_skinCombo->addOption(QskLabelData("Material3"));
    m_skinCombo->setCurrentIndex(0);

    new QskSeparator(Qt::Horizontal, layout);

    // ── Row 3: Color Scheme ──
    auto* row3 = new QskLinearBox(Qt::Horizontal, layout);
    row3->setPreferredHeight(48);
    row3->setSpacing(12);
    auto* schemeLabel = new QskTextLabel("Color Scheme", row3);
    schemeLabel->setPreferredWidth(160);
    m_darkSwitch = new QskSwitchButton(row3);
    auto* schemeVal = new QskTextLabel("Light", row3);
    schemeVal->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    connect(m_darkSwitch, &QskAbstractButton::toggled,
        [schemeVal](bool checked) {
            schemeVal->setText(checked ? "Dark" : "Light");
        });

    new QskSeparator(Qt::Horizontal, layout);

    // ── Row 4: Font Size ──
    auto* row4 = new QskLinearBox(Qt::Horizontal, layout);
    row4->setPreferredHeight(48);
    row4->setSpacing(12);
    auto* fontLabel = new QskTextLabel("Font Size", row4);
    fontLabel->setPreferredWidth(160);
    m_fontScaleCombo = new QskComboBox(row4);
    m_fontScaleCombo->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_fontScaleCombo->addOption(QskLabelData("Small"));
    m_fontScaleCombo->addOption(QskLabelData("Medium"));
    m_fontScaleCombo->addOption(QskLabelData("Large"));
    m_fontScaleCombo->addOption(QskLabelData("Extra Large"));
    m_fontScaleCombo->setCurrentIndex(1);

    new QskSeparator(Qt::Horizontal, layout);

    // ── Row 5: Debug Background ──
    auto* row5 = new QskLinearBox(Qt::Horizontal, layout);
    row5->setPreferredHeight(48);
    row5->setSpacing(12);
    auto* debugLabel = new QskTextLabel("Debug Background", row5);
    debugLabel->setPreferredWidth(160);
    m_debugBgSwitch = new QskSwitchButton(row5);

    new QskSeparator(Qt::Horizontal, layout);

    // ── Row 6: Phone Answer ──
    auto* row6 = new QskLinearBox(Qt::Horizontal, layout);
    row6->setPreferredHeight(48);
    row6->setSpacing(12);
    auto* phoneLabel = new QskTextLabel("Phone Answer", row6);
    phoneLabel->setPreferredWidth(160);
    m_phoneAnswerCombo = new QskComboBox(row6);
    m_phoneAnswerCombo->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_phoneAnswerCombo->addOption(QskLabelData("Disabled"));
    m_phoneAnswerCombo->addOption(QskLabelData("Manual"));
    m_phoneAnswerCombo->addOption(QskLabelData("Auto"));
    m_phoneAnswerCombo->setCurrentIndex(0);

    new QskSeparator(Qt::Horizontal, layout);

    // ── Row 7: Push Notification ──
    auto* row7 = new QskLinearBox(Qt::Horizontal, layout);
    row7->setPreferredHeight(48);
    row7->setSpacing(12);
    auto* pushNotifyLabel = new QskTextLabel("Push Notification", row7);
    pushNotifyLabel->setPreferredWidth(160);
    m_pushNotifySwitch = new QskSwitchButton(row7);

    layout->addStretch(1);
}

SettingsPage::~SettingsPage()
{
    if (s_instance == this)
        s_instance = nullptr;
}

void SettingsPage::changeFontScale(int delta)
{
    QSettings s;
    int idx = qBound(0, s.value("fontScale", 1).toInt() + delta, 3);
    s.setValue("fontScale", idx);

    static const int sizes[][4] = {
        {16, 22, 14, 12}, {21, 29, 19, 16},
        {28, 39, 25, 21}, {35, 48, 32, 27},
    };
    if (sharedFontSizes) {
        sharedFontSizes->body    = sizes[idx][0];
        sharedFontSizes->title   = sizes[idx][1];
        sharedFontSizes->caption = sizes[idx][2];
        sharedFontSizes->global  = sizes[idx][3];
    }
    if (applyAndroidFonts) applyAndroidFonts();

    // Sync SettingsPage combo if open
    if (s_instance && s_instance->m_fontScaleCombo) {
        s_instance->m_fontScaleCombo->blockSignals(true);
        s_instance->m_fontScaleCombo->setCurrentIndex(idx);
        s_instance->m_fontScaleCombo->blockSignals(false);
    }
}

void SettingsPage::onCreate(const QVariantMap&, const QVariantMap&)
{
    s_instance = this;

    // Restore persisted values (before connecting handlers)
    QSettings settings;
    m_transitionCombo->setCurrentIndex(settings.value("transition", 3).toInt());
    m_skinCombo->setCurrentIndex(settings.value("skin", 0).toInt());
    m_darkSwitch->setChecked(settings.value("darkMode", false).toBool());
    m_fontScaleCombo->setCurrentIndex(settings.value("fontScale", 1).toInt());
    m_debugBgSwitch->setChecked(settings.value("debugBackground", false).toBool());
    m_phoneAnswerCombo->setCurrentIndex(settings.value("phoneAnswer", 0).toInt());
    m_pushNotifySwitch->setChecked(settings.value("pushNotification", true).toBool());

    if (m_signalsConnected) return;
    m_signalsConnected = true;

    // ── Connect signal handlers (fire on user interaction, not on restore) ──
    connect(m_transitionCombo, &QskComboBox::currentIndexChanged,
        this, [this](int index) {
            // 跳过同一类型的重复分配（例如从 Perspective 切到 Perspective）
            if (index == m_currentAnimatorIdx) return;
            m_currentAnimatorIdx = index;

            QSettings().setValue("transition", index);
            auto* sb = pageManager() ? pageManager()->stackBox() : nullptr;
            if (!sb) return;
            QskStackBoxAnimator* newAnim = nullptr;
            switch (index) {
                case 0: newAnim = new QskStackBoxAnimator1(sb); break;
                case 1: newAnim = new QskStackBoxAnimator2(sb); break;
                case 2: newAnim = new QskStackBoxAnimator3(sb); break;
                case 3: newAnim = new QskStackBoxAnimator4(sb); break;
            }
            if (newAnim) sb->setAnimator(newAnim);
        });

    connect(m_skinCombo, &QskComboBox::currentIndexChanged,
        this, [](int index) {
            QSettings().setValue("skin", index);
            static const char* names[] = {"Fusion", "Fluent2", "Material3"};
            if (index >= 0 && index < 3) {
                qskSkinManager->setSkin(names[index]);
            }
        });

    connect(m_darkSwitch, &QskAbstractButton::toggled,
        this, [](bool checked) {
            QSettings().setValue("darkMode", checked);
            auto* s = qskSkinManager->skin();
            if (s) {
                s->setColorScheme(checked
                    ? QskSkin::DarkScheme : QskSkin::LightScheme);
            }
        });

    connect(m_fontScaleCombo, &QskComboBox::currentIndexChanged,
        this, [](int index) {
            QSettings().setValue("fontScale", index);
            static const int sizes[][4] = {
                {16, 22, 14, 12},
                {21, 29, 19, 16},
                {28, 39, 25, 21},
                {35, 48, 32, 27},
            };
            if (index >= 0 && index < 4 && SettingsPage::sharedFontSizes) {
                SettingsPage::sharedFontSizes->body    = sizes[index][0];
                SettingsPage::sharedFontSizes->title   = sizes[index][1];
                SettingsPage::sharedFontSizes->caption = sizes[index][2];
                SettingsPage::sharedFontSizes->global  = sizes[index][3];
            }
            if (SettingsPage::applyAndroidFonts) {
                SettingsPage::applyAndroidFonts();
            }
        });

    // ── Row 5: Debug Background toggle ──
    connect(m_debugBgSwitch, &QskAbstractButton::toggled,
        this, [](bool checked) {
            QSettings().setValue("debugBackground", checked);
            QskSetup::setUpdateFlag(
                QskItem::DebugForceBackground, checked);
            qDebug() << "[qsktox] debug background:" << checked;
        });

    // ── Row 6: Phone Answer toggle ──
    connect(m_phoneAnswerCombo, &QskComboBox::currentIndexChanged,
        this, [](int index) {
            QSettings().setValue("phoneAnswer", index);
            PhoneMonitor::setAnswerMode(index);
            qDebug() << "[qsktox] phone answer mode:" << index;
#if defined(Q_OS_ANDROID)
            if (index != 0) {
                QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
                    auto ctx = QNativeInterface::QAndroidApplication::context();
                    QJniObject::callStaticMethod<void>(
                        "io/fedlet/mobutil/PermissionHelper",
                        "requestPhoneCallPermission",
                        "(Landroid/app/Activity;)V",
                        ctx.object());
                });
            }
#endif
        });

    // ── Row 7: Push Notification toggle ──
    connect(m_pushNotifySwitch, &QskAbstractButton::toggled,
        this, [](bool checked) {
            QSettings().setValue("pushNotification", checked);
            qDebug() << "[qsktox] push notification:" << checked;
        });

    // Sync debug background (restored value may differ from QskSetup default)
    if (m_debugBgSwitch->isChecked() != QskSetup::testUpdateFlag(QskItem::DebugForceBackground)) {
        QskSetup::setUpdateFlag(
            QskItem::DebugForceBackground, m_debugBgSwitch->isChecked());
    }
}
