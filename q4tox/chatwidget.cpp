#include "chatwidget.h"
#include "translator.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QLabel>

ChatWidget::ChatWidget(QWidget* parent) 
    : QWidget(parent), chatId(-1), chatType("") {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Header with chat info, dark mode button, and language selector
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerText = new QLabel(_("select_chat_object"));
    headerText->setObjectName("chatHeaderText");
    
    // Dark mode toggle button
    darkModeBtn = new QPushButton("🌙", this);
    darkModeBtn->setToolTip(_("dark_mode"));
    darkModeBtn->setCheckable(true);
    darkModeBtn->setChecked(true); // Default: dark mode
    darkModeBtn->setFixedSize(30, 30);
    connect(darkModeBtn, SIGNAL(clicked()), this, SLOT(onDarkModeClicked()));
    
    langSelector = new QComboBox();
    langSelector->addItem("简体中文", "zh-CN");
    langSelector->addItem("繁體中文", "zh-TW");
    langSelector->addItem("English", "en-US");
    langSelector->setCurrentIndex(0); // Default: zh-CN
    
    headerLayout->addWidget(headerText, 1);
    headerLayout->addWidget(darkModeBtn);
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

void ChatWidget::setHeaderText(const QString& text) {
    if (headerText) {
        headerText->setText(text);
    }
}

void ChatWidget::clearMessages() {
    messageArea->clear();
}

void ChatWidget::appendMessage(const QString& message, const QString& sender) {
    QString color = (sender == "self") ? "#00d4aa" : "#58a6ff";
    QString align = (sender == "self") ? "right" : "left";
    
    // Escape HTML characters manually for Qt4
    QString escaped = message;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    
    QString senderLabel = (sender == "self") ? _("you") : sender;
    QString html = QString("<div style='text-align: %1; margin: 5px;'>"
                         "<span style='color: #888; font-size: 12px;'>%4</span><br>"
                         "<span style='background-color: %2; padding: 5px 10px; "
                         "border-radius: 10px; display: inline-block; max-width: 70%;'>"
                         "%3</span></div>")
                   .arg(align).arg(color).arg(escaped).arg(senderLabel);
    
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

void ChatWidget::onDarkModeClicked() {
    bool dark = darkModeBtn->isChecked();
    ThemeManager::applyTheme(dark);
    darkModeBtn->setText(dark ? "🌙" : "☀️");
    emit darkModeToggled(dark);
}

void ChatWidget::retranslateUi() {
    if (chatId == -1) {
        if (headerText) headerText->setText(_("select_chat_object"));
    }
    if (inputEdit) inputEdit->setPlaceholderText(_("input_placeholder"));
    if (sendBtn) sendBtn->setText(_("buttons.send"));
    if (darkModeBtn) darkModeBtn->setToolTip(_("dark_mode"));
}
