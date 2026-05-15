#include "apilog.h"
#include <cstring>
#include <qdatetime.h>

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
    const char* levelStr = "???";
    switch (level) {
        case ApiLogDebug:   levelStr = "DEBUG"; break;
        case ApiLogInfo:    levelStr = "INFO";  break;
        case ApiLogWarning: levelStr = "WARN";  break;
        case ApiLogError:   levelStr = "ERROR"; break;
    }
    
    QString timeStr = getTimeString(timeFmt);
    QString logMsg = "[" + qFromUtf8(levelStr) + "] "
                   + timeStr + " "
                   + qFromUtf8(shortFilename(file)) + ":"
                   + QString::number(line) + " - "
                   + msg;
    
    // 使用 qWarning（兼容 Qt3/Qt4）
    qWarning("%s", qToUtf8(logMsg).data());
}
