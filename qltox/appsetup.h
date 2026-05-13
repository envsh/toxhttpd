#ifndef APPSETUP_H
#define APPSETUP_H

#include <functional>
#include <map>
#include <vector>

#ifdef QT3_BUILD
#include <qobject.h>
#include <qtimer.h>
#include <qapplication.h>
#include <qtranslator.h>
#else
#include <QObject>
#include <QTimer>
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#endif

class QtappSetup : public QObject {
    Q_OBJECT
public:
    static void setup(QApplication& app);

    static int addTimer(unsigned int intervalMs, std::function<void()> callback);
    static void removeTimer(int timerId);
    static void onExit(std::function<void()> callback);
    // langCode: 应用语言代码格式，如 "zh-CN", "zh-TW", "en-US"
    // 内部自动转为 Qt 格式 "zh_CN", "zh_TW" 并加载对应 .qm 文件
    static void installQtTranslations(const QString& langCode);

private slots:
    void onTimerTimeout();
    void onAppQuit();

private:
    QtappSetup() = default;
    static QtappSetup& inst();
    QtappSetup(const QtappSetup&) = delete;
    QtappSetup& operator=(const QtappSetup&) = delete;

    std::map<int, std::pair<QTimer*, std::function<void()>>> timers_;
    std::vector<std::function<void()>> exitCallbacks_;
    int nextTimerId_ = 1;
};

#endif
