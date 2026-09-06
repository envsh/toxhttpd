#ifndef COMPAT34_H
#define COMPAT34_H

#include "compatcore34.h"

// ========== 平台特有头文件 ==========
#ifdef QT3_BUILD
#include <qpushbt.h>         // QPushButton
#include <qmenubar.h>        // QMenuBar
#include <qpopupmenu.h>      // QPopupMenu
#include <qwidgetstack.h>    // QWidgetStack
#include <qptrlist.h>        // Qt3 原生 QPtrList<T>
#include <qobjectlist.h>     // Qt3 QObjectList（完整定义，用于 children()）
#include <qtextcodec.h>      // QTextCodec
#else
#include <qpushbutton.h>     // QPushButton
#include <QMenuBar>          // QMenuBar
#include <qmenu.h>           // QMenu
#include <qstackedwidget.h>  // QStackedWidget
#include <qstyleoption.h>    // QStyleOption* (Qt4 only)
#include <QTextCodec>        // QTextCodec
#include <qevent.h>          // QMouseEvent 等完整定义
#endif

// ========== 核心 Qt 头文件（被 qlite.pri 继承，无法前向声明） ==========
#include <qlayout.h>         // QBoxLayout, QBoxLayout::Direction
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qcheckbox.h>

// ========== 前向声明（仅用于指针/引用参数） ==========
class QAbstractButton;

// ========== Widgets 兼容函数声明 ==========

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
void qInsertHtml(QTextEdit* edit, const QString& html);
void qClearTextEdit(QTextEdit* edit);
QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border = 0, int autoresize = -1);

// ========== StackedWidget 兼容 ==========
#ifdef QT3_BUILD
typedef QWidgetStack StackedWidget;
#else
typedef QStackedWidget StackedWidget;
#endif

void qStackSetCurrent(StackedWidget* stack, QWidget* page);
void qSetLabelSelectable(QLabel* label);

// ========== Scroll 兼容 ==========
#ifdef QT3_BUILD
#include <qscrollview.h>     // QScrollView
typedef QScrollView ScrollArea;
#else
#include <qscrollarea.h>     // QScrollArea
typedef QScrollArea ScrollArea;
#endif

// ========== 活动窗口检测 ==========
bool qIsAppActive(const QWidget* widget = 0);

void showTempTooltip(QWidget* parent, const QRect& btnRect, const QString& text, int timeoutMs = 3000);

void qActivateWindow(QWidget* w);

#endif  // COMPAT34_H
