#include "messagepage.h"
#include "messagelist.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskBoxShapeMetrics.h>

MessagePage::MessagePage(QQuickItem* parent)
    : Page(parent)
{
}

void MessagePage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    QString channelName = launchArgs.value("channelName").toString();
    Q_UNUSED(channelName)

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── TopBar ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    topBar->setSpacing(8);

    // Back button
    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    backBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    connect(backBtn, &QskAbstractButton::clicked, this, &Page::finish);

    // Channel name title
    auto* title = new QskTextLabel(channelName, topBar);
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    // ── MessageList ──
    m_messageList = new MessageListWidget(layout);
    m_messageList->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_messageList->populateMessages();
}
