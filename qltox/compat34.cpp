#include "compat34.h"
#include <stdlib.h>
#include <unistd.h>

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

// QString API 兼容
QString qTrim(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

QByteArray qToUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}

QString qFromUtf8(const std::string& s) {
    return QString::fromUtf8(s.c_str());
}

QString qFromUtf8(const char* s) {
    return QString::fromUtf8(s);
}

QString qFromUtf8(const char* data, int size) {
    return QString::fromUtf8(data, size);
}

QByteArray qToLocal8Bit(const QString& s) {
#ifdef QT3_BUILD
    return s.local8Bit();
#else
    return s.toLocal8Bit();
#endif
}

int qLastIndexOf(const QString& s, const QString& str) {
#ifdef QT3_BUILD
    return s.findRev(str);
#else
    return s.lastIndexOf(str);
#endif
}

QString qToUpper(const QString& s) {
#ifdef QT3_BUILD
    return s.upper();
#else
    return s.toUpper();
#endif
}

QStringList qSplit(const QString& str, const QString& sep) {
#ifdef QT3_BUILD
    return QStringList::split(sep, str);
#else
    return str.split(sep);
#endif
}

void qSetWindowTitle(QWidget* w, const QString& title) {
#ifdef QT3_BUILD
    w->setCaption(title);
#else
    w->setWindowTitle(title);
#endif
}

void qSetMargins(QBoxLayout* layout, int left, int top, int right, int bottom) {
#ifdef QT3_BUILD
    layout->setMargin(top);
#else
    layout->setContentsMargins(left, top, right, bottom);
#endif
}

#ifdef QT3_BUILD
void qSetChecked(QPushButton* btn, bool checked) {
    btn->setOn(checked);
}
void qSetChecked(QCheckBox* btn, bool checked) {
    btn->setChecked(checked);
}
#else
void qSetChecked(QAbstractButton* btn, bool checked) {
    btn->setChecked(checked);
}
#endif

void qSetCheckable(QPushButton* btn, bool checkable) {
#ifdef QT3_BUILD
    btn->setToggleButton(checkable);
#else
    btn->setCheckable(checkable);
#endif
}

void qSetToolTip(QWidget* w, const QString& tip) {
#ifdef QT3_BUILD
    QToolTip::add(w, tip);
#else
    w->setToolTip(tip);
#endif
}

bool qOpenReadOnly(QFile& file) {
#ifdef QT3_BUILD
    return file.open(IO_ReadOnly);
#else
    return file.open(QIODevice::ReadOnly | QIODevice::Text);
#endif
}

bool qOpenWriteOnly(QFile& file) {
#ifdef QT3_BUILD
    return file.open(IO_WriteOnly);
#else
    return file.open(QIODevice::WriteOnly | QIODevice::Text);
#endif
}

QString qGetHomePath() {
#ifdef QT3_BUILD
    return QDir::homeDirPath();
#else
    return QDir::homePath();
#endif
}

QString qAppDir() {
#ifdef QT3_BUILD
    char exePath[1024];
    int len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        QFileInfo fi;
        fi.setFile(QString(exePath));
        return fi.dirPath(true);
    }
    return ".";
#else
    return QCoreApplication::applicationDirPath();
#endif
}

void qInsertHtml(QTextEdit* edit, const QString& html) {
#ifdef QT3_BUILD
    edit->append(html);  // Qt3 的 append 支持 HTML
#else
    edit->insertHtml(html);
#endif
}

void qClearTextEdit(QTextEdit* edit) {
#ifdef QT3_BUILD
    edit->clear();
#else
    edit->clear();
#endif
}

QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border, int autoresize) {
#ifdef QT3_BUILD
    return new QBoxLayout(parent, dir, border, autoresize, 0);
#else
    QBoxLayout* layout = new QBoxLayout(dir, parent);
    if (border != 0) layout->setContentsMargins(border, border, border, border);
    if (autoresize != -1) layout->setSpacing(autoresize);
    return layout;
#endif
}

QString qFmtTime(uint timestamp) {
#ifdef QT3_BUILD
    QDateTime dt;
    dt.setTime_t(timestamp);
    return dt.toString("yyyy-MM-dd hh:mm:ss");
#else
    return QDateTime::fromTime_t(timestamp).toString("yyyy-MM-dd hh:mm:ss");
#endif
}

QString getCurrentTime() {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

void qStackSetCurrent(StackedWidget* stack, QWidget* page) {
#ifdef QT3_BUILD
    stack->raiseWidget(page);
#else
    stack->setCurrentWidget(page);
#endif
}

// Qt3: 双击 QLabel 复制文本（闪烁 ✓ 反馈）
#ifdef QT3_BUILD
class LabelDblClickFilter : public QObject {
    QLabel* m_label;
    QString m_origText;
    int m_timerId;
public:
    LabelDblClickFilter(QObject* parent)
        : QObject(parent), m_label(nullptr), m_timerId(-1) {}

    bool eventFilter(QObject* obj, QEvent* event) {
        if (event->type() == QEvent::MouseButtonDblClick) {
            QLabel* label = static_cast<QLabel*>(obj);
            if (label && !label->text().isEmpty()) {
                QApplication::clipboard()->setText(label->text());
                if (m_timerId == -1) {
                    m_label = label;
                    m_origText = label->text();
                    label->setText(m_origText + " ✓");
                    m_timerId = startTimer(1000);
                }
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

    void timerEvent(QTimerEvent* event) {
        if (event->timerId() == m_timerId) {
            killTimer(m_timerId);
            m_timerId = -1;
            if (m_label) {
                m_label->setText(m_origText);
                m_label = nullptr;
            }
        }
        QObject::timerEvent(event);
    }
};
#endif

void qSetLabelSelectable(QLabel* label) {
#ifndef QT3_BUILD
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
#else
    label->installEventFilter(new LabelDblClickFilter(label));
#endif
}
