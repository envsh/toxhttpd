#include "settingspage.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>

SettingsPage::SettingsPage(QQuickItem* parent)
    : QskControl(parent)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("\u2190"), topBar);
    backBtn->setPreferredSize(44, 44);
    auto* title = new QskTextLabel("Settings", topBar);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignCenter);

    connect(backBtn, &QskAbstractButton::clicked,
        this, &SettingsPage::backRequested);

    layout->addStretch(1);
    auto* placeholder = new QskTextLabel("Settings coming soon", layout);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addStretch(1);
}
