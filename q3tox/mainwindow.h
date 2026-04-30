#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <qmainwindow.h>
#include <qsplitter.h>
#include "selfinfo.h"
#include "contactlist.h"
#include "chatwidget.h"
#include "eventpoller.h"
#include "api.h"
#include "translator.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    // 静态回调函数，供EventPoller使用
    static void onEventsReceivedStatic(EventList events, void* userData);
    
public slots:
    void onContactSelected(int id, const QString& type);
    void onMessageSent(const QString& message);
    void onLanguageChanged(const QString& langCode);
    void updateUIStrings();
    void handleEvents(const QArray<Event>& events);  // 实际处理事件的成员函数
    
private:
    void loadSelfInfo();
    void loadContacts();
    
    QSplitter* splitter;
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    EventPoller* eventPoller;
    
    int currentChatId;
    QString currentChatType;
};

#endif // MAINWINDOW_H
