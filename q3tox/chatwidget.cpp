#include "chatwidget.h"
#include "translator.h"
#include <qscrollbar.h>
#include <qmessagebox.h>
#include <qlayout.h>

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    QBoxLayout* mainLayout = new QBoxLayout(this, QBoxLayout::TopToBottom, 0, -1, 0);
    mainLayout->setSpacing(0);
    mainLayout->setMargin(0);
    
    // 聊天头部
    QBoxLayout* headerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    headerText = new QLabel(tr("select_chat_object"), this);
    headerLayout->addWidget(headerText, 1);
    
    // 语言选择器
    langSelector = new QComboBox(this);
    langSelector->insertItem(tr("tabs.all"), 0);
    langSelector->insertItem(tr("tabs.friends"), 1);
    langSelector->insertItem(tr("tabs.groups"), 2);
    langSelector->insertItem(tr("tabs.conferences"), 3);
    langSelector->setCurrentItem(0);
    connect(langSelector, SIGNAL(activated(int)), this, SLOT(onLanguageChanged(int)));
    headerLayout->addWidget(langSelector);
    
    mainLayout->addLayout(headerLayout);
    
    // 消息区域
    messageArea = new QTextEdit(this);
    messageArea->setReadOnly(true);
    mainLayout->addWidget(messageArea, 1);
    
    // 输入区域
    QBoxLayout* inputLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    
    inputEdit = new QLineEdit(this);
    inputEdit->setText(tr("placeholders.add_friend"));
    inputLayout->addWidget(inputEdit, 1);
    
    sendBtn = new QPushButton(tr("buttons.send"), this);
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(inputEdit, SIGNAL(returnPressed()), this, SLOT(onSendClicked()));
    inputLayout->addWidget(sendBtn);
    
    mainLayout->addLayout(inputLayout);
}

void ChatWidget::setHeaderText(const QString& text) {
    headerText->setText(text);
}

void ChatWidget::appendMessage(const QString& message, const QString& type, const QString& sender) {
    QString html;
    if (type == "self") {
        html = QString("<p align=\"right\"><span style=\"background: #00d4aa; color: #0d1117; padding: 8px 12px; border-radius: 8px;\">%1</span></p>")
                  .arg(message);
    } else {
        QString senderHtml = sender.isEmpty() ? "" : 
            QString("<div style=\"font-size: 11px; color: #6e7681; margin-bottom: 2px;\">%1</div>").arg(sender);
        html = QString("<p><span style=\"background: #21262d; color: #c9d1d9; padding: 8px 12px; border-radius: 8px;\">%1%2</span></p>")
                  .arg(senderHtml, message);
    }
    
    messageArea->append(html);
}

void ChatWidget::clearMessages() {
    messageArea->clear();
}

void ChatWidget::onSendClicked() {
    QString msg = inputEdit->text().stripWhiteSpace();
    if (msg.isEmpty()) return;
    
    emit messageSent(msg);
    inputEdit->clear();
}

void ChatWidget::onLanguageChanged(int index) {
    QString langCode;
    if (index == 0) langCode = "zh-CN";
    else if (index == 1) langCode = "zh-TW";
    else if (index == 2) langCode = "en-US";
    emit languageChanged(langCode);
}
