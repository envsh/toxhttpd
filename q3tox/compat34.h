#ifndef COMPAT34_H
#define COMPAT34_H

#ifdef QT3_BUILD
// ========== Qt3 头文件 ==========
#include <qapplication.h>
#include <qmainwindow.h>
#include <qpopupmenu.h>
#include <qwidget.h>
#include <qpushbt.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qlistbox.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qmessagebox.h>
#include <qfile.h>
#include <qtextstream.h>
#include <qtextcodec.h>
#include <qdir.h>
#include <qthread.h>
#include <qevent.h>
#include <qtooltip.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qclipboard.h>
#include <qfileinfo.h>
#include <qdatetime.h>
#include <qwidgetstack.h>
#include <unistd.h>
#else
// ========== Qt4 头文件 ==========
#include <QApplication>
#include <QMainWindow>
#include <QMenu>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QTextCodec>
#include <QDir>
#include <QThread>
#include <QEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QClipboard>
#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QStackedWidget>
#endif

// QString API 兼容
inline QString qTrim(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

inline QByteArray qToUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}

// 转本地编码
inline QByteArray qToLocal8Bit(const QString& s) {
#ifdef QT3_BUILD
    return s.local8Bit();
#else
    return s.toLocal8Bit();
#endif
}

// 从后向前查找字符串
inline int qLastIndexOf(const QString& s, const QString& str) {
#ifdef QT3_BUILD
    return s.findRev(str);
#else
    return s.lastIndexOf(str);
#endif
}

// 转大写
inline QString qToUpper(const QString& s) {
#ifdef QT3_BUILD
    return s.upper();
#else
    return s.toUpper();
#endif
}

// 分割字符串 (参数顺序: str在前，sep在后，符合Qt4成员函数习惯)
inline QStringList qSplit(const QString& str, const QString& sep) {
#ifdef QT3_BUILD
    return QStringList::split(sep, str);
#else
    return str.split(sep);
#endif
}

// 窗口标题
inline void qSetWindowTitle(QWidget* w, const QString& title) {
#ifdef QT3_BUILD
    w->setCaption(title);
#else
    w->setWindowTitle(title);
#endif
}

// 布局边距
inline void qSetMargins(QBoxLayout* layout, int left, int top, int right, int bottom) {
#ifdef QT3_BUILD
    layout->setMargin(top);
#else
    layout->setContentsMargins(left, top, right, bottom);
#endif
}

// 按钮状态
#ifdef QT3_BUILD
inline void qSetChecked(QPushButton* btn, bool checked) {
    btn->setOn(checked);
}
inline void qSetChecked(QCheckBox* btn, bool checked) {
    btn->setChecked(checked);
}
#else
inline void qSetChecked(QAbstractButton* btn, bool checked) {
    btn->setChecked(checked);
}
#endif

inline void qSetCheckable(QPushButton* btn, bool checkable) {
#ifdef QT3_BUILD
    btn->setToggleButton(checkable);
#else
    btn->setCheckable(checkable);
#endif
}

// Tooltip
inline void qSetToolTip(QWidget* w, const QString& tip) {
#ifdef QT3_BUILD
    QToolTip::add(w, tip);
#else
    w->setToolTip(tip);
#endif
}

// 文件打开
inline bool qOpenReadOnly(QFile& file) {
#ifdef QT3_BUILD
    return file.open(IO_ReadOnly);
#else
    return file.open(QIODevice::ReadOnly | QIODevice::Text);
#endif
}

inline bool qOpenWriteOnly(QFile& file) {
#ifdef QT3_BUILD
    return file.open(IO_WriteOnly);
#else
    return file.open(QIODevice::WriteOnly | QIODevice::Text);
#endif
}

// 路径
inline QString qGetHomePath() {
#ifdef QT3_BUILD
    return QDir::homeDirPath();
#else
    return QDir::homePath();
#endif
}

// 可执行文件路径
inline QString qAppDir() {
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

// 事件基类
#ifdef QT3_BUILD
typedef QCustomEvent CustomEventBase;
#else
typedef QEvent CustomEventBase;
#endif

// QTextEdit 插入 HTML 兼容
inline void qInsertHtml(QTextEdit* edit, const QString& html) {
#ifdef QT3_BUILD
    edit->append(html);  // Qt3 的 append 支持 HTML
#else
    edit->insertHtml(html);
#endif
}

// QTextEdit 清空兼容
inline void qClearTextEdit(QTextEdit* edit) {
#ifdef QT3_BUILD
    edit->clear();
#else
    edit->clear();
#endif
}

// QBoxLayout 构造函数兼容
inline QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border = 0, int autoresize = -1) {
#ifdef QT3_BUILD
    return new QBoxLayout(parent, dir, border, autoresize, 0);
#else
    QBoxLayout* layout = new QBoxLayout(dir, parent);
    if (border != 0) layout->setContentsMargins(border, border, border, border);
    if (autoresize != -1) layout->setSpacing(autoresize);
    return layout;
#endif
}

// 时间函数声明
QString qFormatTime(const QString& createdAt);
QString qFormatISO8601(const QString& iso8601Str);

// QPtrList 兼容 (Qt3 原生，Qt4 用 QList<T*> 模拟)
#ifndef QT3_BUILD
#include <QList>
template<typename T>
class QPtrList : public QList<T*> {
public:
    void append(T* item) { QList<T*>::append(item); }
    bool isEmpty() const { return QList<T*>::isEmpty(); }
    int count() const { return QList<T*>::count(); }
};
#endif

// StackedWidget (QWidgetStack in Qt3, QStackedWidget in Qt4)
#ifdef QT3_BUILD
typedef QWidgetStack StackedWidget;
#else
typedef QStackedWidget StackedWidget;
#endif

inline void qStackSetCurrent(StackedWidget* stack, QWidget* page) {
#ifdef QT3_BUILD
    stack->raiseWidget(page);
#else
    stack->setCurrentWidget(page);
#endif
}

// 跨平台消息提示音（支持 WAV / Opus）
void playNotifySound(const QString& filePath);
void playNotifySound();

#endif  // COMPAT34_H
