#include "appsetup.h"
#include "compat34.h"

void QtappSetup::setup(QApplication& app) {
    QtappSetup& s = inst();

    // 设置 UTF-8 编解码器
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
//    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));


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

static bool g_quitOnExit = true;

void QtappSetup::setQuitOnExit(bool enabled) {
    g_quitOnExit = enabled;
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
	if (g_quitOnExit) {
		// 但进程不会终止，直到所有非守护线程都结束
		exit(0);
	}
	#endif
}

QtappSetup& QtappSetup::inst() {
    static QtappSetup instance;
    return instance;
}

void QtappSetup::installQtTranslations(const QString& langCode) {
    // langCode 格式: "zh-CN", "zh-TW", "en-US" (与应用翻译文件格式一致)
    // 内部转为 Qt .qm 格式: "zh_CN", "zh_TW"
    qWarning("installQtTranslations: langCode=%s", qToUtf8(langCode).data());
    static QTranslator* translator = nullptr;

    // 移除旧的 translator
    if (translator) {
        qApp->removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    // 英语不需要 Qt 翻译（Qt 默认就是英文）
    if (langCode.startsWith("en"))
        return;

    translator = new QTranslator();

    QString qtLang;
    QString path;
#ifdef QT3_BUILD
    // Qt3: 文件名 qt_zh-cn.qm (小写+连字符)
    // 通过 libraryPaths() 获取 plugins 目录，推导 translations 目录
    qtLang = langCode.lower();
    {
        QStringList libPaths = QApplication::libraryPaths();
        for (const QString& p : libPaths) {
            int idx = p.findRev('/');
            if (idx >= 0) {
                path = p.left(idx) + "/translations";
                if (QDir(path).exists())
                    break;
                path = QString::null;
            }
        }
    }
#else
    // Qt4: 文件名 qt_zh_CN.qm (下划线+大写); 路径通过 QLibraryInfo 定位
    qtLang = langCode;
    qtLang.replace("-", "_");
    path = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif

    QString filePath = path + "/qt_" + qtLang + ".qm";
    if (!path.isEmpty() && translator->load("qt_" + qtLang, path)) {
        qApp->installTranslator(translator);
    } else {
        qWarning("Qt translator not found: %s", qToUtf8(filePath).data());
        delete translator;
        translator = nullptr;
    }
}

extern "C" void qtapp_onExit(void (*callback)()) {
    QtappSetup::onExit(callback);
}

extern "C" void qtapp_installQtTranslations(const char* langCode) {
    QtappSetup::installQtTranslations(qFromUtf8(langCode));
}

extern "C" int qtapp_addTimer(unsigned int intervalMs, void (*callback)()) {
    return QtappSetup::addTimer(intervalMs, callback);
}

extern "C" void qtapp_removeTimer(int timerId) {
    QtappSetup::removeTimer(timerId);
}
