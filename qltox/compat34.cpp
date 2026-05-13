#include "compat34.h"
#include <stdlib.h>

// 时间字符串解析：支持 "yyyy-MM-dd hh:mm:ss" 和 ISO8601 格式
QString qFormatTime(const QString& createdAt) {
    // 如果是 ISO8601 格式（包含 'T'），使用 qFormatISO8601 处理
    if (createdAt.contains('T')) {
        return qFormatISO8601(createdAt);
    }
    
#ifdef QT3_BUILD
    // Qt3: 手动解析，split 后取时间部分
    QStringList parts = QStringList::split(" ", createdAt);
    if (parts.count() >= 2) {
        return parts[1].left(5);  // 取 "hh:mm"
    }
    return QString();
#else
    // Qt4: 使用 QDateTime
    QDateTime dt = QDateTime::fromString(createdAt, "yyyy-MM-dd hh:mm:ss");
    if (dt.isValid()) {
        return dt.toString("hh:mm");
    }
    return QString();
#endif
}

// 解析 ISO8601 时间字符串，返回 "hh:mm" 格式（与 Web 版一致）
QString qFormatISO8601(const QString& iso8601Str) {
    if (iso8601Str.isEmpty()) return QString();
    
#ifdef QT3_BUILD
    // Qt3: 手动解析 ISO8601 "yyyy-MM-ddThh:mm:ssZ" 或 "yyyy-MM-ddThh:mm:ss+08:00"
    QString str = iso8601Str;
    // 移除 'T'
    int tPos = str.find('T');
    if (tPos >= 0) {
        str = str.left(tPos) + " " + str.mid(tPos + 1);
    }
    // 移除时区后缀：'+' 或 '-'（排除日期中的 '-')
    int zonePos = str.find('+');
    if (zonePos < 0) zonePos = str.find('-', 10);
    if (zonePos >= 0) {
        str = str.left(zonePos);
    }
    // 移除 'Z' 后缀（UTC 标识）
    if (str.right(1) == "Z") {
        str = str.left(str.length() - 1);
    }
    // 移除秒后的小数点
    int dotPos = str.find('.');
    if (dotPos >= 0) {
        str = str.left(dotPos);
    }
    // 现在格式应为 "yyyy-MM-dd hh:mm:ss"，取时间部分
    QStringList parts = QStringList::split(" ", str);
    if (parts.count() >= 2) {
        return parts[1].left(5);  // 返回 "hh:mm"（与 Web 版一致）
    }
    return QString();
#else
    // Qt4: 使用 QDateTime 解析 ISO8601
    QDateTime dt = QDateTime::fromString(iso8601Str, Qt::ISODate);
    if (!dt.isValid()) {
        // 尝试移除时区中的 ':'
        QString cleaned = iso8601Str;
        int plusPos = cleaned.indexOf('+');
        int minusPos = cleaned.lastIndexOf('-', cleaned.length() - 1);
        int tzPos = (plusPos > 0) ? plusPos : ((minusPos > 10) ? minusPos : -1);
        if (tzPos > 0) {
            cleaned = cleaned.left(tzPos);
        }
        cleaned.replace(".", "");
        dt = QDateTime::fromString(cleaned, "yyyy-MM-ddTHh:mm:ss");
    }
    if (dt.isValid()) {
        return dt.toString("hh:mm");  // 返回 "hh:mm"（与 Web 版一致）
    }
    return QString();
#endif
}

// 跨平台消息提示音（WAV / Opus）
// Linux:   paplay (PulseAudio, 支持 Opus/WAV) → fallback aplay (ALSA, 仅 WAV)
// macOS:   afplay
// Windows: PowerShell SoundPlayer (仅 WAV, Opus 静默忽略)
void playNotifySound(const QString& filePath) {
    QString cmd;
#if defined(Q_OS_WIN)
    if (!filePath.endsWith(".opus"))
        cmd = QString("powershell -c (New-Object Media.SoundPlayer '%1').PlaySync()").arg(filePath);
#elif defined(Q_OS_MAC)
    cmd = QString("afplay \"%1\" &").arg(filePath);
#else
    if (filePath.endsWith(".opus"))
        cmd = QString("paplay \"%1\" &").arg(filePath);
    else
        cmd = QString("( which paplay >/dev/null 2>&1 && paplay \"%1\" ) || aplay \"%1\" &").arg(filePath);
#endif
    if (!cmd.isEmpty())
        system(qToUtf8(cmd).data());
}

void playNotifySound() {
    playNotifySound("notification.opus");
}
