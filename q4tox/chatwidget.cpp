#include "chatwidget.h"
#include "translator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QLabel>

ChatWidget::ChatWidget(QWidget* parent) 
    : QWidget(parent), chatId(-1) {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Header with chat info and language selector
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* headerText = new QLabel(_("select_chat_object"));
    headerText->setObjectName("chatHeaderText");
    
    langSelector = new QComboBox();
    langSelector->addItem("简体中文", "zh-CN");
    langSelector->addItem("繁體中文", "zh-TW");
    langSelector->addItem("English", "en-US");
    langSelector->setCurrentIndex(0); // Default: zh-CN
    
    headerLayout->addWidget(headerText, 1);
    headerLayout->addWidget(langSelector);
    mainLayout->addLayout(headerLayout);
    
    // Message area
    messageArea = new QTextEdit(this);
    messageArea->setReadOnly(true);
    messageArea->setStyleSheet("border: none; background-color: #1e1e1e; color: #d4d4d4;");
    mainLayout->addWidget(messageArea, 1);
    
    // Input area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputEdit = new QLineEdit();
    inputEdit->setPlaceholderText(_("input_placeholder"));
    
    sendBtn = new QPushButton(_("buttons.send"));
    sendBtn->setFixedWidth(80);
    
    inputLayout->addWidget(inputEdit, 1);
    inputLayout->addWidget(sendBtn);
    mainLayout->addLayout(inputLayout);
    
    // Connect signals
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(inputEdit, SIGNAL(returnPressed()), this, SLOT(onSendClicked()));
    connect(langSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(onLanguageChanged(int)));
}

void ChatWidget::setChatInfo(int id, const QString& type) {
    chatId = id;
    chatType = type;
    
    // Update header text
    QLabel* headerText = findChild<QLabel*>("chatHeaderText");
    if (headerText) {
        if (type == "friend") {
            headerText->setText(_("chat_with_friend").arg(QString::number(id)));
        } else if (type == "conference") {
            headerText->setText(_("chat_with_conference").arg(QString::number(id)));
        } else {
            headerText->setText(_("select_chat_object"));
        }
    }
}

void ChatWidget::appendMessage(const QString& message, bool isSelf) {
    QString color = isSelf ? "#00d4aa" : "#58a6ff";
    QString align = isSelf ? "right" : "left";
    
    // Escape HTML characters manually for Qt4
    QString escaped = message;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    
    QString html = QString("<div style='text-align: %1; margin: 5px;'>"
                         "<span style='background-color: %2; padding: 5px 10px; "
                         "border-radius: 10px; display: inline-block; max-width: 70%;'>"
                         "%3</span></div>")
                   .arg(align).arg(color).arg(escaped);
    
    messageArea->append(html);
    
    // Auto scroll to bottom
    QScrollBar* vbar = messageArea->verticalScrollBar();
    vbar->setValue(vbar->maximum());
}

void ChatWidget::onSendClicked() {
    QString msg = inputEdit->text().trimmed();
    if (msg.isEmpty() || chatId == -1) return;
    
    inputEdit->clear();
    emit messageSent(msg);
}

void ChatWidget::onLanguageChanged(int index) {
    QString langCode = langSelector->itemData(index).toString();
    emit languageChanged(langCode);
}
