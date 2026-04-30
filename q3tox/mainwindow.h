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
    
    void customEvent(QCustomEvent* event);
    
public slots:
    void onContactSelected(int id, const QString& type);
    void onMessageSent(const QString& message);
    void onLanguageChanged(const QString& langCode);
    void retranslateUi();
    void handleEvents(const EventList& events);
    
private:
    
    QSplitter* splitter;
    SelfInfoWidget* selfInfoWidget;
    ContactListWidget* contactListWidget;
    ChatWidget* chatWidget;
    EventPoller* eventPoller;
    
    int currentChatId;
    QString currentChatType;
};

#endif // MAINWINDOW_H
