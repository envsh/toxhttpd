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
    
    // 消息区域
    messageArea = new QTextEdit(this);
    messageArea->setReadOnly(true);
#ifndef QT3_BUILD
    messageArea->setAcceptRichText(true);
#endif
    mainLayout->addWidget(messageArea, 1);
    
    // 输入区域
    QBoxLayout* inputLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    
    inputEdit = new QLineEdit(this);
    inputEdit->setText(_("placeholders.add_friend"));
    inputLayout->addWidget(inputEdit, 1);
    
    sendBtn = new QPushButton(_("buttons.send"), this);
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(inputEdit, SIGNAL(returnPressed()), this, SLOT(onSendClicked()));
    inputLayout->addWidget(sendBtn);
    
    mainLayout->addLayout(inputLayout);
}

void ChatWidget::setHeaderText(const QString& text) {
    headerText->setText(text);
}

void ChatWidget::appendMessage(const QString& message, const QString& type, 
                              const QString& sender, const QString& time,
                              const QString& avatarText) {
    // 构建头像 HTML（预留 img 接口）
    QString avatarHtml;
    if (!avatarText.isEmpty()) {
        QString displayLetter = qToUpper(avatarText.left(1));
        avatarHtml = QString(
            "<div style=\"width:48px;height:48px;border-radius:50%;background:#30363d;overflow:hidden;\">"
            "  <!-- 预留头像接口：未来替换为 <img src=\"avatar_path\"> -->"
            "  <div style=\"color:#8b949e;font-size:18px;font-weight:500;text-align:center;line-height:48px;\">%1</div>"
            "</div>"
        ).arg(displayLetter);
    } else {
        avatarHtml = 
            "<div style=\"width:48px;height:48px;border-radius:50%;background:#30363d;\"></div>";
    }
    
    QString html;
    if (type == "self") {
        // 自己消息：内容列（右对齐），头像列在右
        html = QString(
            "<table width=\"100%\" cellpadding=\"4\" cellspacing=\"0\" border=\"0\" style=\"margin-bottom:8px;\">"
            "<tr>"
            "  <td align=\"right\" valign=\"top\" style=\"width:100%;\">"
            "    <div style=\"font-size:11px;color:#c9d1d9;margin-bottom:2px;\">"
            "      <span style=\"font-weight:500;margin:0 8px;\">Me</span>"
            "      <span style=\"font-size:10px;color:#8b949e;\">%1</span>"
            "    </div>"
            "    <div style=\"background:#21262d;color:#c9d1d9;padding:8px 12px;border-radius:8px;width:80%;text-align:left;word-wrap:break-word;word-break:break-all;\">%2</div>"
            "  </td>"
            "  <td width=\"48\" valign=\"top\" style=\"padding-left:8px;\">%3</td>"
            "</tr>"
            "</table>"
        ).arg(time).arg(message).arg(avatarHtml);
    } else {
        // 对方消息：头像左列，内容右列
        QString senderHtml = sender.isEmpty() ? "Peer" : sender;
        html = QString(
            "<table width=\"100%\" cellpadding=\"4\" cellspacing=\"0\" border=\"0\" style=\"margin-bottom:8px;\">"
            "<tr>"
            "  <td width=\"48\" valign=\"top\" style=\"padding-right:8px;\">%4</td>"
            "  <td valign=\"top\">"
            "    <div style=\"font-size:11px;color:#c9d1d9;margin-bottom:2px;\">"
            "      <span style=\"font-weight:500;margin-right:8px;\">%1</span>"
            "      <span style=\"font-size:10px;color:#8b949e;\">%2</span>"
            "    </div>"
            "    <div style=\"background:#21262d;color:#c9d1d9;padding:8px 12px;border-radius:8px;width:80%;word-wrap:break-word;word-break:break-all;\">%3</div>"
            "  </td>"
            "</tr>"
            "</table>"
        ).arg(senderHtml).arg(time).arg(message).arg(avatarHtml);
    }
    
    qInsertHtml(messageArea, html);
}

void ChatWidget::clearMessages() {
    messageArea->clear();
}

void ChatWidget::onSendClicked() {
    QString msg = qTrim(inputEdit->text());
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
        QString text = inputEdit->text();
        if (text == "输入 Tox ID 添加好友" || 
            text == "Enter Tox ID to add friend" ||
            text == "輸入 Tox ID 添加好友") {
            inputEdit->setText(_("placeholders.add_friend"));
        }
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
