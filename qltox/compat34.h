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
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qevent.h>
#include <qtooltip.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qclipboard.h>
#include <qfileinfo.h>
#include <qdatetime.h>
#include <qwidgetstack.h>
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
#include <QMutex>
#include <QWaitCondition>
#include <QEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QClipboard>
#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QStackedWidget>
#endif

// QString API 兼容
QString qTrim(const QString& s);
QByteArray qToUtf8(const QString& s);
QString qFromUtf8(const std::string& s);
QString qFromUtf8(const char* s);
QString qFromUtf8(const char* data, int size);
QByteArray qToLocal8Bit(const QString& s);
int qLastIndexOf(const QString& s, const QString& str);
QString qToUpper(const QString& s);
QStringList qSplit(const QString& str, const QString& sep);

// 窗口标题
void qSetWindowTitle(QWidget* w, const QString& title);

// 布局边距
void qSetMargins(QBoxLayout* layout, int left, int top, int right, int bottom);

// 按钮状态
#ifdef QT3_BUILD
void qSetChecked(QPushButton* btn, bool checked);
void qSetChecked(QCheckBox* btn, bool checked);
#else
void qSetChecked(QAbstractButton* btn, bool checked);
#endif

void qSetCheckable(QPushButton* btn, bool checkable);

// Tooltip
void qSetToolTip(QWidget* w, const QString& tip);

// 文件打开
bool qOpenReadOnly(QFile& file);
bool qOpenWriteOnly(QFile& file);

// 路径
QString qGetHomePath();

// 可执行文件路径
QString qAppDir();

// 事件基类
#ifdef QT3_BUILD
typedef QCustomEvent CustomEventBase;
#else
typedef QEvent CustomEventBase;
#endif

// QTextEdit 插入 HTML 兼容
void qInsertHtml(QTextEdit* edit, const QString& html);

// QTextEdit 清空兼容
void qClearTextEdit(QTextEdit* edit);

// QBoxLayout 构造函数兼容
QBoxLayout* qNewBoxLayout(QWidget* parent, QBoxLayout::Direction dir, int border = 0, int autoresize = -1);

// 时间函数声明
QString qFormatTime(const QString& createdAt);
QString qFormatISO8601(const QString& iso8601Str);

// Unix 时间戳格式化（跨 Qt3/Qt4）
QString qFmtTime(uint timestamp);

// 当前时间字符串
QString getCurrentTime();

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

void qStackSetCurrent(StackedWidget* stack, QWidget* page);

// 让 QLabel 可选中的兼容宏（Qt4: 鼠标拖动选中; Qt3: 双击复制）
void qSetLabelSelectable(QLabel* label);

#endif  // COMPAT34_H
