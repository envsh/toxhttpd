#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "compat34.h"
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "eventpoller.h"
#include "api.h"
#include "translator.h"
#include "friendinfodialog.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void customEvent(CustomEventBase* event);
    
    public slots:
    void onContactSelected(int id, const QString& type);
    void onMessageSent(const QString& message);
    void onLanguageChanged(const QString& langCode);
    void retranslateUi();
    void handleEvents(const EventList& events);
    void onViewInfoRequested(int id, const QString& type);
    void onDeleteOrLeaveRequested(int id, const QString& type);
    void onInviteToConferenceRequested(int friendId);
    
private:
    
    QSplitter* splitter;
    QWidget* sidebarWidget;  // 左侧边栏容器
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    EventPoller* eventPoller;
    
    int currentChatId;
    QString currentChatType;
};

#endif // MAINWINDOW_H
