#include "compat34.h"
#include <stdlib.h>
#include <unistd.h>

// ========== 实现所需的 Qt 头文件 ==========
#include <qwidget.h>         // QWidget (setWindowTitle, setToolTip, newBoxLayout)
#include <qfile.h>           // QFile, QIODevice (qOpenReadOnly/WriteOnly)
#include <qdir.h>            // QDir (qGetHomePath)
#include <qdatetime.h>       // QDateTime (qFmtTime, getCurrentTime)
#include <qapplication.h>    // QApplication::clipboard (LabelDblClickFilter)
#include <qclipboard.h>      // QClipboard (LabelDblClickFilter)

#ifdef QT3_BUILD
#include <qtooltip.h>        // QToolTip::add
#include <qfileinfo.h>       // QFileInfo (qAppDir)
#include <qwidgetlist.h>     // QWidgetList (topLevelWidgets)
#include <qimage.h>          // QImage (qX11SetWmIcon)
#include <X11/Xlib.h>        // Display, XChangeProperty, XFlush
#include <X11/Xatom.h>       // XA_CARDINAL (qX11SetWmIcon)
// X11 的宏会污染 QEvent 的枚举值, 必须 undef
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#include <qdesktopwidget.h>  // QApplication::desktop (TipLabel::placeTip)
#include <qtimer.h>          // QTimer::singleShot (showTempTooltip)
#else
#include <QToolTip>          // QToolTip::showText (showTempTooltip)
#include <QTimer>            // QTimer::singleShot (showTempTooltip)
#endif

// ========== EventType34 ==========
EventType34 toEventType34(int raw) {
#ifdef QT3_BUILD
    return raw;
#else
    return static_cast<QEvent::Type>(raw);
#endif
}

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
    if (iso8601Str.isEmpty()) { return QString(); }
    
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
    if (zonePos < 0) { zonePos = str.find('-', 10); }
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

#ifdef QT3_BUILD
static void qX11SetWmIcon(const QPixmap& pm) {
    QImage img = pm.convertToImage();
    if (img.isNull()) { return; }

    const int iconSize = 32;
    QImage scaled = img.smoothScale(iconSize, iconSize);

    const int dataSize = 2 + iconSize * iconSize;
    unsigned long* data = new unsigned long[dataSize];
    data[0] = (unsigned long)iconSize;
    data[1] = (unsigned long)iconSize;
    for (int y = 0; y < iconSize; ++y) {
        for (int x = 0; x < iconSize; ++x) {
            QRgb px = scaled.pixel(x, y);
            data[2 + y * iconSize + x] =
                ((unsigned long)qAlpha(px) << 24) |
                ((unsigned long)qRed(px) << 16) |
                ((unsigned long)qGreen(px) << 8) |
                (unsigned long)qBlue(px);
        }
    }

    Display* dpy = QPaintDevice::x11AppDisplay();
    if (!dpy) { delete[] data; return; }
    Atom netWmIcon = XInternAtom(dpy, "_NET_WM_ICON", False);
    QWidgetList* topWidgets = qApp->topLevelWidgets();
    for (uint i = 0; i < uint(topWidgets->count()); ++i) {
        WId wid = topWidgets->at(i)->winId();
        if (wid) {
            XChangeProperty(dpy, wid, netWmIcon, XA_CARDINAL, 32,
                          PropModeReplace, (unsigned char*)data, dataSize);
        }
    }
    XFlush(dpy);
    delete[] data;
}
#endif

void qSetAppIcon(const char** xpm) {
    QPixmap pm(xpm);
#ifdef QT3_BUILD
    if (QWidget* mw = qApp->mainWidget()) {
        mw->setIcon(pm);
    } else {
        QWidgetList* list = qApp->topLevelWidgets();
        if (list->count() == 0) {
            qWarning("qSetAppIcon: no top-level widgets found");
            return;
        }
        for (uint i = 0; i < uint(list->count()); ++i) {
            list->at(i)->setIcon(pm);
        }
    }
    qX11SetWmIcon(pm);
#else
    qApp->setWindowIcon(QIcon(pm));
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
    if (border != 0) { layout->setContentsMargins(border, border, border, border); }
    if (autoresize != -1) { layout->setSpacing(autoresize); }
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

// ========== 活动窗口检测 ==========
bool qIsAppActive(const QWidget* widget) {
    if (widget) {
#ifdef QT3_BUILD
        return qApp->activeWindow() == widget->topLevelWidget();
#else
        return widget->isActiveWindow();
#endif
    }
    return qApp->activeWindow() != 0;
}

// ========== URL 打开兼容 ==========
#ifdef QT3_BUILD
#include <stdlib.h>
void qOpenUrl(const QString& url) {
#if defined(Q_OS_WIN32)
    system(QString("cmd /c start \"\" \"" + url + "\"").local8Bit().data());
#elif defined(Q_OS_MACX)
    system(QString("open \"" + url + "\"").local8Bit().data());
#else
    system(QString("xdg-open '" + url + "'").local8Bit().data());
#endif
}
#else
#include <QDesktopServices>
#include <QUrl>
void qOpenUrl(const QString& url) {
    QDesktopServices::openUrl(QUrl(url));
}
#endif

#ifdef QT3_BUILD
namespace {

class TipLabel : public QLabel {
public:
    static TipLabel* instance;

    TipLabel()
        : QLabel(0, "tip_label",
            Qt::WStyle_Customize | Qt::WStyle_Tool
            | Qt::WStyle_NoBorder | Qt::WStyle_StaysOnTop)
        , m_widget(0)
        , m_expireTimerId(0)
        , m_hideTimerId(0)
    {
        delete instance;
        instance = this;

        QPalette pal;
        pal.setColor(QPalette::Active, QColorGroup::Background, QColor(255, 255, 220));
        pal.setColor(QPalette::Disabled, QColorGroup::Background, QColor(255, 255, 220));
        pal.setColor(QPalette::Inactive, QColorGroup::Background, QColor(255, 255, 220));
        pal.setColor(QPalette::Active, QColorGroup::Foreground, Qt::black);
        pal.setColor(QPalette::Disabled, QColorGroup::Foreground, Qt::black);
        pal.setColor(QPalette::Inactive, QColorGroup::Foreground, Qt::black);
        setPalette(pal);
        setMargin(3);
        setFrameStyle(QFrame::Box | QFrame::Plain);
        setLineWidth(1);
        setAlignment(AlignLeft);
        setIndent(1);
        qApp->installEventFilter(this);
        setMouseTracking(true);
    }

    ~TipLabel() {
        instance = 0;
    }

    void showTip(QWidget* parent, const QRect& btnRect,
                 const QString& text, int timeoutMs) {
        if (text.isEmpty()) {
            hideTip();
            return;
        }

        QRect screenRect(parent->mapToGlobal(btnRect.topLeft()), btnRect.size());

        bool changed = (text != QLabel::text())
                    || (parent != m_widget)
                    || (m_btnRect != btnRect);

        if (changed) {
            setText(text);
            m_widget = parent;
            m_btnRect = btnRect;
            m_trackingRect = screenRect;
            adjustSize();
            placeTip();
            if (!isVisible())
                show();
            restartExpireTimer(timeoutMs);
        } else {
            m_trackingRect = screenRect;
            restartExpireTimer(timeoutMs);
        }
    }

    void hideTipImmediately() {
        if (m_hideTimerId) { killTimer(m_hideTimerId); m_hideTimerId = 0; }
        if (m_expireTimerId) { killTimer(m_expireTimerId); m_expireTimerId = 0; }
        close();
        m_btnRect = QRect();
        m_trackingRect = QRect();
        m_widget = 0;
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        switch (event->type()) {
        case QEvent::MouseMove: {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (!m_btnRect.isNull() && !m_trackingRect.contains(me->globalPos()))
                hideTip();
            break;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
            hideTipImmediately();
            break;
        case QEvent::Leave:
            hideTip();
            break;
        default:
            break;
        }
        return false;
    }

    void timerEvent(QTimerEvent* e) override {
        if (e->timerId() == m_hideTimerId) {
            killTimer(m_hideTimerId);
            m_hideTimerId = 0;
            hideTipImmediately();
        } else if (e->timerId() == m_expireTimerId) {
            killTimer(m_expireTimerId);
            m_expireTimerId = 0;
            hideTip();
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_btnRect.isNull()) {
            QPoint pos = e->globalPos();
            if (m_widget)
                pos = m_widget->mapFromGlobal(pos);
            if (!m_btnRect.contains(pos))
                hideTip();
        }
        QLabel::mouseMoveEvent(e);
    }

private:
    void placeTip() {
        QPoint center = m_widget->mapToGlobal(m_btnRect.center());
        QPoint pos2(center.x() - width() / 2, center.y() - height() - 4);

        QRect screen = QApplication::desktop()->screenGeometry(center);
        if (pos2.x() < screen.x())
            pos2.setX(screen.x() + 2);
        if (pos2.x() + width() > screen.x() + screen.width())
            pos2.setX(screen.x() + screen.width() - width() - 2);
        if (pos2.y() < screen.y())
            pos2.setY(screen.y() + 2);

        move(pos2);
    }

    void restartExpireTimer(int timeoutMs) {
        if (m_expireTimerId) killTimer(m_expireTimerId);
        if (m_hideTimerId) { killTimer(m_hideTimerId); m_hideTimerId = 0; }
        m_expireTimerId = startTimer(timeoutMs > 0 ? timeoutMs : 3000);
    }

    void hideTip() {
        if (!m_hideTimerId)
            m_hideTimerId = startTimer(300);
    }

    QWidget* m_widget;
    QRect m_btnRect;
    QRect m_trackingRect;
    int m_expireTimerId;
    int m_hideTimerId;
};

TipLabel* TipLabel::instance = 0;

} // namespace
#endif

void showTempTooltip(QWidget* parent, const QRect& btnRect,
                     const QString& text, int timeoutMs) {
#ifdef QT3_BUILD
    if (text.isEmpty()) {
        if (TipLabel::instance)
            TipLabel::instance->hideTipImmediately();
        return;
    }
    if (!TipLabel::instance)
        new TipLabel();
    TipLabel::instance->showTip(parent, btnRect, text, timeoutMs);
#else
    QToolTip::showText(parent->mapToGlobal(btnRect.center()), text, parent, btnRect);
#endif
}

void qSleepMs(unsigned long ms) {
    usleep(ms * 1000);
}

QByteArray base64Decode(const std::string& b64) {
    if (b64.empty()) return QByteArray();
#ifdef QT3_BUILD
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char rev[256] = {};
    for (int i = 0; i < 64; ++i) rev[(unsigned char)alphabet[i]] = i;
    rev[(unsigned char)'-'] = 62;
    rev[(unsigned char)'_'] = 63;

    int len = (int)b64.size();
    int maxOut = ((len + 3) / 4) * 3;
    QByteArray out;
    out.resize(maxOut);
    int wp = 0;
    unsigned char buf[4];
    int bufPos = 0;

    for (int i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)b64[i];
        if (c == '=') {
            break;
        }
        unsigned char val = rev[c];
        if (val == 0 && c != 'A') {
            if (c != '\n' && c != '\r' && c != ' ' && c != '\t')
                return QByteArray();
            continue;
        }
        buf[bufPos++] = val;
        if (bufPos == 4) {
            out[wp++] = (char)((buf[0] << 2) | (buf[1] >> 4));
            out[wp++] = (char)((buf[1] << 4) | (buf[2] >> 2));
            out[wp++] = (char)((buf[2] << 6) | buf[3]);
            bufPos = 0;
        }
    }
    if (bufPos > 0) {
        for (int i = bufPos; i < 4; ++i) buf[i] = 0;
        if (bufPos >= 2)
            out[wp++] = (char)((buf[0] << 2) | (buf[1] >> 4));
        if (bufPos >= 3)
            out[wp++] = (char)((buf[1] << 4) | (buf[2] >> 2));
    }
    out.resize(wp);
    return out;
#else
    return QByteArray::fromBase64(QByteArray(b64.data(), (int)b64.size()));
#endif
}
