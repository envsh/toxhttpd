#ifndef COMPAT34_H
#define COMPAT34_H

#ifdef QT3_BUILD
// ========== Qt3 头文件 ==========
#include <qapplication.h>
#include <qmainwindow.h>
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
#include <unistd.h>
#else
// ========== Qt4 头文件 ==========
#include <QApplication>
#include <QMainWindow>
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
inline void qSetChecked(QPushButton* btn, bool checked) {
#ifdef QT3_BUILD
    btn->setOn(checked);
#else
    btn->setChecked(checked);
#endif
}

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

#endif  // COMPAT34_H
