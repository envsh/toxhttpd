#include "chatwidget.h"
#include "translator.h"
#include "compat34.h"

#include "ThemeManager.h"

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 0, 0);
    mainLayout->setSpacing(0);
    qSetMargins(mainLayout, 0, 0, 0, 0);
    
    // 聊天头部
    QBoxLayout* headerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    headerText = new QLabel(_("select_chat_object"), this);
    headerLayout->addWidget(headerText, 1);
    
    // 语言选择器
    langSelector = new QComboBox(this);
#ifdef QT3_BUILD
    langSelector->insertItem(QString::fromUtf8("简体中文"), 0);
    langSelector->insertItem(QString::fromUtf8("繁體中文"), 1);
    langSelector->insertItem(QString::fromUtf8("English"), 2);
    langSelector->setCurrentItem(0);
#else
    langSelector->insertItem(0, QString::fromUtf8("简体中文"));
    langSelector->insertItem(1, QString::fromUtf8("繁體中文"));
    langSelector->insertItem(2, QString::fromUtf8("English"));
    langSelector->setCurrentIndex(0);
#endif
    connect(langSelector, SIGNAL(activated(int)), this, SLOT(onLanguageChanged(int)));
    headerLayout->addWidget(langSelector);

    // 主题切换复选框
    themeCheckBox = new QCheckBox(_("theme_dark"), this);
    connect(themeCheckBox, SIGNAL(toggled(bool)), this, SLOT(onThemeToggled(bool)));
    headerLayout->addWidget(themeCheckBox);
    
    mainLayout->addLayout(headerLayout);
    
    // 消息区域（虚拟化列表）
    messageArea = new ChatView(this);
    mainLayout->addWidget(messageArea, 1);
    
    // 输入区域 (2行 x 3列)
#ifdef QT3_BUILD
    QGridLayout* inputGrid = new QGridLayout(2, 3, 2);
    inputEdit = new MessageInput(this);
    inputGrid->addMultiCellWidget(inputEdit, 0, 1, 0, 0);
    emojiBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x98\x8A"), this);
    inputGrid->addWidget(emojiBtn, 0, 1);
    fileBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x81"), this);
    inputGrid->addWidget(fileBtn, 1, 1);
    sendBtn = new QPushButton(_("buttons.send"), this);
    int h = inputEdit->fontMetrics().lineSpacing() * 2 + 6;
    sendBtn->setFixedSize(h, h);
    inputGrid->addMultiCellWidget(sendBtn, 0, 1, 2, 2);
    inputGrid->setColStretch(0, 1);
#else
    QGridLayout* inputGrid = new QGridLayout();
    inputGrid->setSpacing(2);
    inputEdit = new MessageInput(this);
    inputGrid->addWidget(inputEdit, 0, 0, 2, 1);
    emojiBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x98\x8A"), this);
    inputGrid->addWidget(emojiBtn, 0, 1);
    fileBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x93\x81"), this);
    inputGrid->addWidget(fileBtn, 1, 1);
    sendBtn = new QPushButton(_("buttons.send"), this);
    int h = inputEdit->fontMetrics().lineSpacing() * 2 + 6;
    sendBtn->setFixedSize(h, h);
    inputGrid->addWidget(sendBtn, 0, 2, 2, 1);
    inputGrid->setColumnStretch(0, 1);
#endif
    
#ifdef QT3_BUILD
    inputEdit->setText(_("placeholders.add_friend"));
#else
    inputEdit->setPlainText(_("placeholders.add_friend"));
#endif
    
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(inputEdit, SIGNAL(sendRequested()), this, SLOT(onSendClicked()));
    connect(emojiBtn, SIGNAL(clicked()), this, SLOT(onEmojiClicked()));
    connect(fileBtn, SIGNAL(clicked()), this, SLOT(onFileClicked()));
    connect(inputEdit, SIGNAL(filePasteRequested(QString)), this, SLOT(onFilePaste(QString)));
    
    mainLayout->addLayout(inputGrid);
}

void ChatWidget::setHeaderText(const QString& text) {
    headerText->setText(text);
}

void ChatWidget::appendMessage(const QString& message, const QString& type, 
                              const QString& sender, const QString& time,
                              const QString& avatarText) {
    ChatMessage msg;
    msg.messageText = message;
    msg.type = type;
    msg.sender = sender.isEmpty() ? "Peer" : sender;
    msg.time = time;
    msg.avatarText = avatarText;
    messageArea->appendMessage(msg);
}

void ChatWidget::clearMessages() {
    messageArea->clearMessages();
}

void ChatWidget::onSendClicked() {
#ifdef QT3_BUILD
    QString msg = qTrim(inputEdit->text());
#else
    QString msg = qTrim(inputEdit->toPlainText());
#endif
    if (msg.isEmpty()) return;
    
    emit messageSent(msg);
    inputEdit->clear();
}

void ChatWidget::retranslateUi() {
    // 更新聊天头
    // 注意：headerText 的更新由 MainWindow::retranslateUi() 处理
    
    // 更新按钮文字
    if (sendBtn) sendBtn->setText(_("buttons.send"));
    
    // 更新主题复选框文本
    if (themeCheckBox) themeCheckBox->setText(_("theme_dark"));
    
    // 更新输入框 placeholder（如果当前显示的是默认值）
    if (inputEdit) {
#ifdef QT3_BUILD
        QString text = inputEdit->text();
        if (text == "输入 Tox ID 添加好友" || 
            text == "Enter Tox ID to add friend" ||
            text == "輸入 Tox ID 添加好友") {
            inputEdit->setText(_("placeholders.add_friend"));
        }
#else
        QString text = inputEdit->toPlainText();
        if (text == "输入 Tox ID 添加好友" || 
            text == "Enter Tox ID to add friend" ||
            text == "輸入 Tox ID 添加好友") {
            inputEdit->setPlainText(_("placeholders.add_friend"));
        }
#endif
    }
}

void ChatWidget::onLanguageChanged(int index) {
    QString langCode;
    if (index == 0) langCode = "zh-CN";
    else if (index == 1) langCode = "zh-TW";
    else if (index == 2) langCode = "en-US";
    else langCode = "zh-CN"; // 默认
    qWarning("ChatWidget: language changed to %s", qToUtf8(langCode).data());
    emit languageChanged(langCode);
}

void ChatWidget::onThemeToggled(bool checked) {
    // TODO: 实现主题切换逻辑
    qWarning("Theme toggled: %d", checked);
    ThemeManager::applyTheme(checked);
}

void ChatWidget::onEmojiClicked() {
    // TODO: open emoji picker
    qWarning("onEmojiClicked: not implemented");
}

void ChatWidget::onFileClicked() {
    // TODO: open file dialog and send file
    qWarning("onFileClicked: not implemented");
}

void ChatWidget::onFilePaste(const QString& filePath) {
    // TODO: actually send the file
    qWarning("onFilePaste: %s", qToUtf8(filePath).data());
}
