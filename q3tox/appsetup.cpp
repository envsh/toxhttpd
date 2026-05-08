#include "appsetup.h"

void QtappSetup::setup(QApplication& app) {
    QtappSetup& s = inst();

    QObject::connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));

#ifdef QT3_BUILD
    QObject::connect(&app, SIGNAL(lastWindowClosed()), &s, SLOT(onAppQuit()));
#else
    QObject::connect(&app, SIGNAL(aboutToQuit()), &s, SLOT(onAppQuit()));
#endif
}

int QtappSetup::addTimer(unsigned int intervalMs, std::function<void()> callback) {
    QtappSetup& s = inst();
    QTimer* timer = new QTimer(&s);
    int id = s.nextTimerId_++;
    s.timers_[id] = {timer, std::move(callback)};
	QObject::connect(timer, SIGNAL(timeout()), &s, SLOT(onTimerTimeout()));

	#if QT_VERSION >= 0x040000
    timer->setInterval(intervalMs);
    timer->start();
	#else
	timer->start(intervalMs);  // start with interval directly
	#endif

    return id;
}

void QtappSetup::removeTimer(int timerId) {
    QtappSetup& s = inst();
    auto it = s.timers_.find(timerId);
    if (it != s.timers_.end()) {
        it->second.first->stop();
        delete it->second.first;
        s.timers_.erase(it);
    }
}

void QtappSetup::onExit(std::function<void()> callback) {
    inst().exitCallbacks_.push_back(std::move(callback));
}

void QtappSetup::onTimerTimeout() {
    for (auto& pair : timers_) {
        if (pair.second.first == sender()) {
            pair.second.second();
            return;
        }
    }
}

void QtappSetup::onAppQuit() {
    for (auto& cb : exitCallbacks_) {
        cb();
    }
    exitCallbacks_.clear();

	#if QT_VERSION < 0x050000
	// 但进程不会终止，直到所有非守护线程都结束
	exit(0);
	#endif
}

QtappSetup& QtappSetup::inst() {
    static QtappSetup instance;
    return instance;
}

extern "C" void qtapp_onExit(void (*callback)()) {
    QtappSetup::onExit(callback);
}

extern "C" int qtapp_addTimer(unsigned int intervalMs, void (*callback)()) {
    return QtappSetup::addTimer(intervalMs, callback);
}

extern "C" void qtapp_removeTimer(int timerId) {
    QtappSetup::removeTimer(timerId);
}
