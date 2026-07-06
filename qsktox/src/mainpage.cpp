#include "mainpage.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskTextField.h>
#include <QskPushButton.h>
#include <QskMenu.h>
#include <QskBoxShapeMetrics.h>
#include <QskLabelData.h>

MainPage::MainPage(QQuickItem* parent)
    : QskControl(parent)
{
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

    // Options menu (popup)
    auto* optionsMenu = new QskMenu(topBar);
    optionsMenu->setOrigin(QPointF(0, 56));
    optionsMenu->addOption(QskLabelData("Settings"));
    optionsMenu->addSeparator();
    optionsMenu->addOption(QskLabelData("About"));
    optionsMenu->addSeparator();
    optionsMenu->addOption(QskLabelData("Logout"));

    connect(optionsBtn, &QskAbstractButton::clicked, [optionsMenu]() {
        optionsMenu->open();
    });
    connect(optionsMenu, &QskMenu::triggered, this,
        [this, optionsMenu](int index) {
            if (index == 0)
                emit settingsRequested();
            else if (index == 2)
                emit aboutRequested();
            else if (index == 4)
                emit logoutRequested();
            optionsMenu->close();
        });

    // ── ChatArea ──
    auto* chatArea = new QskLinearBox(Qt::Vertical, layout);
    chatArea->setPanel(true);
    chatArea->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Expanding);

    chatArea->addStretch(1);
    auto* welcome = new QskTextLabel("Welcome to qsktox", chatArea);
    welcome->setAlignment(Qt::AlignCenter);
    chatArea->addStretch(1);

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
