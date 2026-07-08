#include "chatwidget.h"
#include "translator.h"
#include "compat34.h"
#include "restapi.h"
#include "translate_util.h"
#ifdef QT3_BUILD
#include <qtimer.h>
#else
#include <QTimer>
#endif

#include "config.h"
#include "ThemeManager.h"
#ifdef QT3_BUILD
#include <qfiledialog.h>
#else
#include <QFileDialog>
#endif

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    if (s_autoTranslateArg) { m_autoTranslateEnabled = true; }
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 0, 0);
    mainLayout->setSpacing(0);
    qSetMargins(mainLayout, 0, 0, 0, 0);
    
    // 聊天头部
    QBoxLayout* headerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    headerText = new QLabel(_("select_chat_object"), this);
    {
        QFont hf = headerText->font();
        hf.setPointSize(hf.pointSize() + 2);
        hf.setBold(true);
        headerText->setFont(hf);
    }
    headerLayout->addWidget(headerText, 1);

    // 加载状态条（语言选择器左侧）
    m_loadingBar = new LoadingBar(this);
    headerLayout->addWidget(m_loadingBar, 0);

    // 语言选择器
    langSelector = new QComboBox(this);
#ifdef QT3_BUILD
    langSelector->insertItem(qFromUtf8("简体中文"), 0);
    langSelector->insertItem(qFromUtf8("繁體中文"), 1);
    langSelector->insertItem(qFromUtf8("English"), 2);
    langSelector->setCurrentItem(0);
#else
    langSelector->insertItem(0, qFromUtf8("简体中文"));
    langSelector->insertItem(1, qFromUtf8("繁體中文"));
    langSelector->insertItem(2, qFromUtf8("English"));
    langSelector->setCurrentIndex(0);
#endif
    connect(langSelector, SIGNAL(activated(int)), this, SLOT(onUilangChanged(int)));
    connect(langSelector, SIGNAL(activated(int)), this, SLOT(onTranslateTolangChanged(int)));
    headerLayout->addWidget(langSelector);

    // 风格选择器
    m_styleSelector = new QComboBox(this);
    {
        const auto& styles = StyleParams::registeredStyles();
#ifdef QT3_BUILD
        for (int i = 0; i < (int)styles.size(); ++i)
            m_styleSelector->insertItem(_(styles[i].displayKey), i);
        int cur = 0;
        for (int i = 0; i < (int)styles.size(); ++i)
            if (QString(styles[i].id) == QString(ThemeManager::styleId())) { cur = i; break; }
        m_styleSelector->setCurrentItem(cur);
#else
        for (int i = 0; i < (int)styles.size(); ++i)
            m_styleSelector->insertItem(i, _(styles[i].displayKey));
        int cur = 0;
        for (int i = 0; i < (int)styles.size(); ++i)
            if (QString(styles[i].id) == QString(ThemeManager::styleId())) { cur = i; break; }
        m_styleSelector->setCurrentIndex(cur);
#endif
    }
    connect(m_styleSelector, SIGNAL(activated(int)), this, SLOT(onStyleChanged(int)));
    headerLayout->addWidget(m_styleSelector);

    // 主题切换复选框
    themeCheckBox = new QCheckBox(_("theme_dark"), this);
    qSetChecked(themeCheckBox, true);
    connect(themeCheckBox, SIGNAL(toggled(bool)), this, SLOT(onThemeToggled(bool)));
    headerLayout->addWidget(themeCheckBox);
    
    mainLayout->addLayout(headerLayout);
    
    // ── 未读消息横幅 ──
    m_unreadBanner = new QLabel(this);
    m_unreadBanner->setAlignment(Qt::AlignCenter);
    m_unreadBanner->setFixedHeight(26);
    m_unreadBanner->hide();
    mainLayout->addWidget(m_unreadBanner);
    
    // 消息区域（虚拟化列表）
    messageArea = new ChatView(this);
    mainLayout->addWidget(messageArea, 1);
    connect(messageArea, SIGNAL(translateClicked(int)), this, SLOT(onTranslateClicked(int)));
    connect(messageArea, SIGNAL(sourceClicked(int)), this, SIGNAL(sourceClicked(int)));
    connect(messageArea, SIGNAL(retryClicked(int, const QString&, const QString&)), this, SIGNAL(retryClicked(int, const QString&, const QString&)));
    connect(messageArea, SIGNAL(resendMessage(int)), this, SIGNAL(resendMessage(int)));
    connect(messageArea, SIGNAL(openFullSizeImage(int, const QString&)),
            this, SIGNAL(openFullSizeImage(int, const QString&)));
    connect(messageArea, SIGNAL(mentionClicked(const QString&)), this, SLOT(onMentionClicked(const QString&)));
    connect(messageArea, SIGNAL(autoTranslateRequested(int, const QString&, const QString&)),
            this, SLOT(onAutoTranslateRequested(int, const QString&, const QString&)));
    
    // 输入区域 (2行 x 3列)
#ifdef QT3_BUILD
    QGridLayout* inputGrid = new QGridLayout(2, 3, 2);
    inputEdit = new MessageInput(this);
    inputGrid->addMultiCellWidget(inputEdit, 0, 1, 0, 0);
    emojiBtn = new EmojiPushButton(qFromUtf8("😊"), this);
    emojiBtn->setFixedSize(24, 24);
    inputGrid->addWidget(emojiBtn, 0, 1);
    fileBtn = new EmojiPushButton(qFromUtf8("📎"), this);
    fileBtn->setFixedSize(24, 24);
    inputGrid->addWidget(fileBtn, 1, 1);
    sendBtn = new QPushButton(_("buttons.send"), this);
    QFontMetrics fm = inputEdit->fontMetrics();
    int twoLineH = fm.lineSpacing() * 2 + fm.lineSpacing() / 2 + 6;
    inputEdit->setMaximumHeight(twoLineH);
    sendBtn->setFixedSize(twoLineH, twoLineH);
    inputGrid->addMultiCellWidget(sendBtn, 0, 1, 2, 2);
    inputGrid->setColStretch(0, 1);

    m_sendEnBtn = new QPushButton("Send EN", this);
    m_sendEnBtn->setFixedWidth(60);
    m_sendEnBtn->setFixedHeight(twoLineH);
    inputGrid->addMultiCellWidget(m_sendEnBtn, 0, 1, 3, 3);
#else
    QGridLayout* inputGrid = new QGridLayout();
    inputGrid->setSpacing(2);
    inputEdit = new MessageInput(this);
    inputGrid->addWidget(inputEdit, 0, 0, 2, 1);
    emojiBtn = new EmojiPushButton(qFromUtf8("😊"), this);
    emojiBtn->setFixedSize(24, 24);
    inputGrid->addWidget(emojiBtn, 0, 1);
    fileBtn = new EmojiPushButton(qFromUtf8("📎"), this);
    fileBtn->setFixedSize(24, 24);
    inputGrid->addWidget(fileBtn, 1, 1);
    sendBtn = new QPushButton(_("buttons.send"), this);
    QFontMetrics fm = inputEdit->fontMetrics();
    int twoLineH = fm.lineSpacing() * 2 + fm.lineSpacing() / 2 + 6;
    inputEdit->setMaximumHeight(twoLineH);
    sendBtn->setFixedSize(twoLineH, twoLineH);
    inputGrid->addWidget(sendBtn, 0, 2, 2, 1);
    inputGrid->setColumnStretch(0, 1);

    m_sendEnBtn = new QPushButton("Send EN", this);
    m_sendEnBtn->setFixedWidth(60);
    m_sendEnBtn->setFixedHeight(twoLineH);
    inputGrid->addWidget(m_sendEnBtn, 0, 3, 2, 1);
#endif

        inputEdit->setPlaceholderText(_("placeholders.type_message"));

    // Emoji picker
    emojiPicker = new EmojiPicker(this);
    connect(emojiPicker, SIGNAL(emojiSelected(const QString&)), this, SLOT(onEmojiInsert(const QString&)));
    
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(m_sendEnBtn, SIGNAL(clicked()), this, SLOT(onSendEnClicked()));
    connect(inputEdit, SIGNAL(sendRequested()), this, SLOT(onSendClicked()));
    connect(emojiBtn, SIGNAL(clicked()), this, SLOT(onEmojiClicked()));
    connect(fileBtn, SIGNAL(clicked()), this, SLOT(onFileClicked()));
    connect(inputEdit, SIGNAL(filePasteRequested(const QString&)), this, SLOT(onFilePaste(const QString&)));
    
    mainLayout->addLayout(inputGrid);
}

void ChatWidget::setHeaderText(const QString& text) {
    m_baseHeader = text;
    int cnt = messageArea->messageCount();
    if (cnt > 0)
        headerText->setText(text + QString(" (%1)").arg(cnt));
    else
        headerText->setText(text);
}

void ChatWidget::showUnreadBanner(int count) {
    if (count <= 0) { hideUnreadBanner(); return; }
    QColor bg(200, 220, 255);
    QColor fg(30, 30, 30);
#ifdef QT3_BUILD
    m_unreadBanner->setPaletteBackgroundColor(bg);
    m_unreadBanner->setPaletteForegroundColor(fg);
#else
    QPalette p;
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, fg);
    m_unreadBanner->setPalette(p);
    m_unreadBanner->setAutoFillBackground(true);
#endif
    m_unreadBanner->setText(QString("  %1  ").arg(_("n_new_messages").arg(count)));
    m_unreadBanner->show();
    QTimer::singleShot(5000, this, SLOT(hideUnreadBanner()));
}

void ChatWidget::hideUnreadBanner() {
    m_unreadBanner->hide();
}

void ChatWidget::updateHeaderCount() {
    setHeaderText(m_baseHeader);
}

void ChatWidget::appendMessage(const QString& message, const QString& type, 
                              const QString& senderName,
                              const QString& senderNickname,
                              int peerNumber,
                              const QString& time, const QString& avatarUrl,
                              const QString& ipAddress) {
    ChatElement msg;
    msg.messageText = message;
    msg.category = type;
    msg.senderName = senderName;
    msg.senderNickname = senderNickname;
    msg.peerNumber = peerNumber;
    msg.time = time;
    msg.avatarUrl = avatarUrl;
    msg.ipAddress = ipAddress;
    messageArea->appendMessage(msg);
    updateHeaderCount();
}

void ChatWidget::clearMessages() {
    messageArea->clearMessages();
    updateHeaderCount();
}

int ChatWidget::messageCount() const {
    return messageArea->messageCount();
}

ChatElement ChatWidget::messageAt(int index) const {
    return messageArea->messageAt(index);
}

ChatElement& ChatWidget::mutableMessageAt(int index) {
    return messageArea->messageAt(index);
}

void ChatWidget::triggerRelayout(int msgIndex) {
    messageArea->triggerRelayout(msgIndex);
}

void ChatWidget::appendMessage(const ChatElement& msg) {
    messageArea->appendMessage(msg);
    updateHeaderCount();
}

void ChatWidget::restoreMessages(const std::vector<ChatElement>& msgs) {
    messageArea->restoreMessages(msgs);
    updateHeaderCount();
}

std::vector<ChatElement> ChatWidget::detachMessages() {
    return messageArea->detachMessages();
}

void ChatWidget::attachMessages(std::vector<ChatElement> msgs) {
    messageArea->attachMessages(std::move(msgs));
    updateHeaderCount();
}

void ChatWidget::onSendClicked() {
    if (inputEdit->placeholderText().length() > 0) {
#ifdef QT3_BUILD
        if (inputEdit->text() == inputEdit->placeholderText()) { return; }
#else
        if (inputEdit->toPlainText() == inputEdit->placeholderText()) { return; }
#endif
    }
#ifdef QT3_BUILD
    QString msg = qTrim(inputEdit->text());
#else
    QString msg = qTrim(inputEdit->toPlainText());
#endif
    if (msg.isEmpty()) return;
    
    inputEdit->saveToHistory(msg);
    emit messageSent(msg);
#ifdef QT3_BUILD
    inputEdit->selectAll();
    inputEdit->removeSelectedText();
#else
    QTextCursor cursor = inputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
#endif
}

void ChatWidget::onSendEnClicked() {
#ifdef QT3_BUILD
    QString msg = qTrim(inputEdit->text());
#else
    QString msg = qTrim(inputEdit->toPlainText());
#endif
    if (msg.isEmpty()) return;

    inputEdit->saveToHistory(msg);
    appendMessage(msg, "self", "Me", QString(), -1, getCurrentTime());
#ifdef QT3_BUILD
    inputEdit->selectAll();
    inputEdit->removeSelectedText();
#else
    QTextCursor cursor = inputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
#endif
    emit translateForSendRequested(msg, "en");
}

void ChatWidget::retranslateUi() {
    // 更新聊天头
    // 注意：headerText 的更新由 MainWindow::retranslateUi() 处理
    
    // 更新按钮文字
    if (sendBtn) { sendBtn->setText(_("buttons.send")); }
    
    // 更新主题复选框文本
    if (themeCheckBox) { themeCheckBox->setText(_("theme_dark")); }
    
    // 更新风格选择器文字
    if (m_styleSelector) {
        const auto& styles = StyleParams::registeredStyles();
        for (int i = 0; i < (int)styles.size(); ++i) {
#ifdef QT3_BUILD
            m_styleSelector->changeItem(_(styles[i].displayKey), i);
#else
            m_styleSelector->setItemText(i, _(styles[i].displayKey));
#endif
        }
    }
    
    // 更新输入框 placeholder
    if (inputEdit) {
    inputEdit->setPlaceholderText(_("placeholders.type_message"));
    }
}

static QString langCodeFromIndex(int index) {
    if (index == 0) return "zh-CN";
    if (index == 1) return "zh-TW";
    if (index == 2) return "en-US";
    return "zh-CN";
}

void ChatWidget::onUilangChanged(int index) {
    QString langCode = langCodeFromIndex(index);
    Config::setValue("uilang", langCode);
    qWarning("ChatWidget: uilang changed to %s", qToUtf8(langCode).data());
    emit languageChanged(langCode);
}

void ChatWidget::onTranslateTolangChanged(int index) {
    Config::setValue("translate_tolang", langCodeFromIndex(index));
}

void ChatWidget::onTranslateClicked(int msgIndex) {
    ChatElement& msg = messageArea->messageAt(msgIndex);
    if (msg.translationInProgress) { return; }

    // Toggle: if already translated, just toggle display
    if (!msg.translatedText.isEmpty()) {
        msg.showTranslation = !msg.showTranslation;
        messageArea->triggerRelayout(msgIndex);
        return;
    }

    msg.translateError = QString();
    msg.translationInProgress = true;
    messageArea->triggerRelayout(msgIndex);
    emit translateRequested(msgIndex, msg.messageText,
                            Config::value("translate_tolang"));
}

void ChatWidget::onAutoTranslateRequested(int msgIndex, const QString& text, const QString& toLang) {
    if (!m_autoTranslateEnabled) { return; }
    qWarning("ChatWidget: auto-translate request msgIndex=%d toLang=%s text=[%.80s]",
             msgIndex, qToUtf8(toLang).data(), qToUtf8(text).data());
    if (msgIndex < 0 || msgIndex >= (int)messageArea->messageCount()) { return; }
    ChatElement& msg = messageArea->messageAt(msgIndex);
    if (msg.translationInProgress || !msg.translatedText.isEmpty()) { return; }
    msg.translateError = QString();
    msg.translationInProgress = true;
    messageArea->triggerRelayout(msgIndex);
    emit translateRequested(msgIndex, text, toLang);
}

void ChatWidget::onTranslateResult(int msgIndex, bool success, const QString& translatedText, const QString& errorMessage) {
    if (msgIndex < 0 || msgIndex >= (int)messageArea->messageCount()) return;
    ChatElement& msg = messageArea->messageAt(msgIndex);
    msg.translationInProgress = false;
    msg.autoTranslatePending = false;
    if (success) {
        msg.translatedText = translatedText;
        msg.showTranslation = true;
        msg.translateError = QString();
    } else {
        msg.translateError = errorMessage;
    }
    messageArea->triggerRelayout(msgIndex);
}

void ChatWidget::onThemeToggled(bool checked) {
    // TODO: 实现主题切换逻辑
    qWarning("Theme toggled: %d", checked);
    ThemeManager::applyTheme(checked);
}

void ChatWidget::onStyleChanged(int index) {
    const auto& styles = StyleParams::registeredStyles();
    if (index >= 0 && index < (int)styles.size())
        ThemeManager::setStyle(styles[index].id, ThemeManager::isDarkMode());
}

void ChatWidget::onEmojiClicked() {
    QPoint btnPos = emojiBtn->mapToGlobal(QPoint(0, 0));
    int pickerW = emojiPicker->width();
    int pickerH = emojiPicker->height();
    int x = btnPos.x() + emojiBtn->width() - pickerW;
    int y = btnPos.y() - pickerH;
    if (y < 0) { y = btnPos.y() + emojiBtn->height(); }
    emojiPicker->showAt(QPoint(x, y));
}

void ChatWidget::onEmojiInsert(const QString& emoji) {
    qWarning("ChatWidget::onEmojiInsert: received emoji string of length %d: '%s'", emoji.length(), qToUtf8(emoji).data());
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->insert(emoji);
#else
    inputEdit->insertPlainText(emoji);
#endif
    inputEdit->setFocus();
}

void ChatWidget::onFileClicked() {
    QString path;
#ifdef QT3_BUILD
    path = QFileDialog::getOpenFileName(QString::null, QString::null, this);
#else
    path = QFileDialog::getOpenFileName(this, QString(), QString());
#endif
    if (path.isEmpty()) { return; }
    emit fileSendRequested(path);
}

void ChatWidget::onFilePaste(const QString& filePath) {
    if (filePath.isEmpty()) { return; }
    emit fileSendRequested(filePath);
}

bool ChatWidget::s_autoTranslateArg = false;

void ChatWidget::onMentionClicked(const QString& username) {
    QString mention = "@" + username + " ";
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->insert(mention);
#else
    inputEdit->insertPlainText(mention);
#endif
    inputEdit->setFocus();
}
