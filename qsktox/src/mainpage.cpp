#include "mainpage.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskTextField.h>
#include <QskPushButton.h>

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

    topBar->addStretch(1);
    auto* title = new QskTextLabel("qsktox", topBar);
    title->setAlignment(Qt::AlignCenter);
    topBar->addStretch(1);

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

    auto* sendBtn = new QskPushButton("→", inputBar);
    sendBtn->setPreferredWidth(48);
    sendBtn->setPreferredHeight(40);
    inputBar->addSpacer(8, 0);
}
