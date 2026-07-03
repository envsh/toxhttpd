#include "limelog.h"
#include <cstring>
#include <qdatetime.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static const char* shortFilename(const char* file) {
    const char* p = file + std::strlen(file);
    while (p > file && *(p-1) != '/' && *(p-1) != '\\') {
        --p;
    }
    return p;
}

// 获取格式化时间字符串，兼容 Qt3/Qt4
static QString getTimeString(ApiLogTimeFormat timeFmt) {
    QString timeStr;
#ifdef QT3_BUILD
    QTime nowTime = QTime::currentTime();
    QDate nowDate = QDate::currentDate();
    switch (timeFmt) {
        case ApiTimeFull:
            timeStr = nowDate.toString("yyyy-MM-dd ") + nowTime.toString("hh:mm:ss.zzz");
            break;
        case ApiTimeShort:
            timeStr = nowTime.toString("hh:mm:ss");
            break;
        case ApiTimeCustom:
            timeStr = nowTime.toString("hh:mm:ss"); // 默认简短时间
            break;
    }
#else
    QDateTime now = QDateTime::currentDateTime();
    switch (timeFmt) {
        case ApiTimeFull:
            timeStr = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
            break;
        case ApiTimeShort:
            timeStr = now.toString("hh:mm:ss");
            break;
        case ApiTimeCustom:
            timeStr = now.toString("hh:mm:ss"); // 默认简短时间
            break;
    }
#endif
    return timeStr;
}

void apiLogImpl(ApiLogLevel level, ApiLogTimeFormat timeFmt, const char* file, int line, const QString& msg) {
    // 一次检测 stderr 是否为终端
    static bool s_tty = false;
    static bool s_checked = false;
    if (!s_checked) {
#ifdef _WIN32
        s_tty = _isatty(_fileno(stderr)) != 0;
#else
        s_tty = isatty(fileno(stderr)) != 0;
#endif
        s_checked = true;
    }

    const char* levelStr = "???";
    const char* startCode = "";
    switch (level) {
        case ApiLogDebug:   levelStr = "DEBUG"; startCode = "\033[2m";     break;
        case ApiLogInfo:    levelStr = "INFO";  startCode = "\033[1;32m";  break;
        case ApiLogWarning: levelStr = "WARN";  startCode = "\033[1;33m";  break;
        case ApiLogError:   levelStr = "ERROR"; startCode = "\033[1;31m";  break;
    }
    
    QString timeStr = getTimeString(timeFmt);
    QString logMsg;
    if (s_tty) {
        logMsg = "\033[2m[" + timeStr + "]\033[0m "
               + qFromUtf8(startCode) + "[" + qFromUtf8(levelStr) + "]\033[0m "
               + "- "
               + msg + " "
               + "\033[2m[" + qFromUtf8(shortFilename(file)) + ":"
               + QString::number(line) + "]\033[0m";
    } else {
        logMsg = "[" + timeStr + "] ["
               + qFromUtf8(levelStr) + "] "
               + "- "
               + msg + " ["
               + qFromUtf8(shortFilename(file)) + ":"
               + QString::number(line) + "]";
    }
    
    // 直接输出 stderr，绕过 Qt3 下 qthooks.cpp 对 qWarning 的自定义 hook（避免重复级别/时间戳）
    fprintf(stderr, "%s\n", qToUtf8(logMsg).data());
    fflush(stderr);
}
