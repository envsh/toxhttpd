#ifndef COMPAT34_H
#define COMPAT34_H

// ========== 跨 Qt3/Qt4 通用头文件 ==========
#include <qstring.h>         // QString, QByteArray, QStringList
#include <qevent.h>          // QEvent (Qt4), QCustomEvent (Qt3)
#include <qlayout.h>         // QBoxLayout, QBoxLayout::Direction
#include <qlist.h>           // QList — for QPtrList

// ========== 平台特有头文件 ==========
#ifdef QT3_BUILD
#include <qpushbt.h>         // QPushButton
#include <qmenubar.h>        // QMenuBar
#include <qpopupmenu.h>      // QPopupMenu
#include <qwidgetstack.h>    // QWidgetStack
#include <qptrlist.h>        // Qt3 原生 QPtrList<T>
#else
#include <qpushbutton.h>     // QPushButton
#include <QMenuBar>          // QMenuBar
#include <qmenu.h>           // QMenu
#include <qstackedwidget.h>  // QStackedWidget
#include <qstyleoption.h>    // QStyleOption* (Qt4 only)
#endif

// ========== 核心 Qt 头文件（被 qlite.pri 继承，无法前向声明） ==========
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qcheckbox.h>
#include <qfile.h>

// ========== 前向声明（仅用于指针/引用参数） ==========
class QAbstractButton;

// ========== 事件类型兼容 ==========
#ifdef QT3_BUILD
typedef int EventType34;
#else
typedef QEvent::Type EventType34;
#endif

EventType34 toEventType34(int raw);

// ========== 事件基类 ==========
#ifdef QT3_BUILD
typedef QCustomEvent CustomEventBase;
#else
typedef QEvent CustomEventBase;
#endif

// ========== 兼容函数声明 ==========

QString qTrim(const QString& s);
QByteArray qToUtf8(const QString& s);
QString qFromUtf8(const std::string& s);
QString qFromUtf8(const char* s);
QString qFromUtf8(const char* data, int size);
QByteArray qToLocal8Bit(const QString& s);
int qLastIndexOf(const QString& s, const QString& str);
QString qToUpper(const QString& s);
QStringList qSplit(const QString& str, const QString& sep);

void qSetWindowTitle(QWidget* w, const QString& title);
void qSetAppIcon(const char** xpm);
#if QT_VERSION < 0x050500
/// qInfo 实现见 qthooks.cpp (与 qDebug/qWarning/qFatal 的 hook 放一起)
void qInfo(const char* fmt, ...);
#endif
void qSetMargins(QBoxLayout* layout, int left, int top, int right, int bottom);

#ifdef QT3_BUILD
void qSetChecked(QPushButton* btn, bool checked);
void qSetChecked(QCheckBox* btn, bool checked);
#else
void qSetChecked(QAbstractButton* btn, bool checked);
#endif

void qSetCheckable(QPushButton* btn, bool checkable);
void qSetToolTip(QWidget* w, const QString& tip);
bool qOpenReadOnly(QFile& file);
bool qOpenWriteOnly(QFile& file);
QString qGetHomePath();
QString qAppDir();
void qInsertHtml(QTextEdit* edit, const QString& html);
void qClearTextEdit(QTextEdit* edit);
QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border = 0, int autoresize = -1);

QString qFormatTime(const QString& createdAt);
QString qFormatISO8601(const QString& iso8601Str);
QString qFmtTime(uint timestamp);
QString getCurrentTime();

// ========== QPtrList 兼容（Qt4 模拟） ==========
#ifndef QT3_BUILD
template<typename T>
class QPtrList : public QList<T*> {
public:
    void append(T* item) { QList<T*>::append(item); }
    bool isEmpty() const { return QList<T*>::isEmpty(); }
    int count() const { return QList<T*>::count(); }
};
#endif

// ========== StackedWidget 兼容 ==========
#ifdef QT3_BUILD
typedef QWidgetStack StackedWidget;
#else
typedef QStackedWidget StackedWidget;
#endif

void qStackSetCurrent(StackedWidget* stack, QWidget* page);
void qSetLabelSelectable(QLabel* label);

// ========== 活动窗口检测 ==========
bool qIsAppActive(const QWidget* widget = 0);

void qOpenUrl(const QString& url);

#endif  // COMPAT34_H
