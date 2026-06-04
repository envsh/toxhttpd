#ifndef EMBEDDEDMENUBAR_H
#define EMBEDDEDMENUBAR_H

/*
 * EmbeddedMenuBar — 继承 QMenuBar，封装 Qt3/Qt4 差异。
 * 让 QMenuBar 可以在任意 QWidget 内正常工作（不依赖 QMainWindow）。
 * 适用：frameless 窗口、自绘标题栏、sidebar、popup 等。
 *
 * 使用示例
 * ========
 *   EmbeddedMenuBar* mb = new EmbeddedMenuBar(parent);
 *   mb->init();                          // ① 字体 + margins
 *
 *   MenuWidget34* file = mb->addMenu("文件(&F)");          // ② 顶级菜单
 *   EmbeddedMenuBar::addItem(file, "新建", this, SLOT(onNew()));
 *   EmbeddedMenuBar::addSeparator(file);
 *   EmbeddedMenuBar::addItem(file, "退出", this, SLOT(close()));
 *
 *   MenuWidget34* edit = mb->addMenu("编辑(&E)");
 *   EmbeddedMenuBar::addItem(edit, "撤销", this, SLOT(onUndo()));
 *
 *   MenuWidget34* recent = mb->addSubMenu(file, "最近打开"); // 子菜单
 *   EmbeddedMenuBar::addItem(recent, "文档1", this, SLOT(onOpen()));
 *
 *   mb->finalize();                       // ③ 后置修复（最后调用）
 *
 *   // 放入布局
 *   layout->addWidget(mb, 0);
 *
 *
 * API 速查
 * ========
 *   init(pointSize)          — 设字号 + Qt3 setMargin / Qt4 setContentsMargins
 *   addMenu(title)           — 添加顶级菜单，返回 MenuWidget34*
 *   addSubMenu(parent,title) — 给已有菜单加子菜单
 *   addItem(menu,text,receiver,slot)  静态 — 加菜单项并连接 slot
 *   addSeparator(menu)      静态 — 加分隔线
 *   finalize()               — 完后调用（Qt3 width fix / Qt4 QWindowsStyle等）
 *
 * 注意
 * ====
 *   - finalize() 必须在所有菜单添加完毕后调用。
 *   - addItem/addSeparator 是 static，不需要实例指针。
 *   - MenuWidget34 = Qt3 QPopupMenu* / Qt4 QMenu*。
 *   - Qt4 自动设 QWindowsStyle + setNativeMenuBar(false)。
 *   - Qt3 自动补偿 PM_MenuBarItemSpacing 差值。
 *   - 子菜单字体自动继承 menubar 字体。
 */

#include "compat34.h"

#ifdef QT3_BUILD
#include <qstyle.h>
typedef QPopupMenu MenuWidget34;
#else
#include <QStyleFactory>
#include <qstyle.h>
typedef QMenu MenuWidget34;
#endif

class EmbeddedMenuBar : public QMenuBar {
public:
    explicit EmbeddedMenuBar(QWidget* parent = 0)
        : QMenuBar(parent) {}

    void init(int pointSize = 10) {
        QFont f = font();
        f.setPointSize(pointSize);
        setFont(f);
#ifdef QT3_BUILD
        setMargin(0);
#else
        setContentsMargins(3, 0, 3, 0);
#endif
    }

    MenuWidget34* addMenu(const QString& title) {
#ifdef QT3_BUILD
        QPopupMenu* menu = new QPopupMenu(this);
        menu->setFont(font());
        insertItem(title, menu);
        return menu;
#else
        QMenu* menu = QMenuBar::addMenu(title);
        menu->setFont(font());
        return menu;
#endif
    }

    MenuWidget34* addSubMenu(MenuWidget34* parent, const QString& title) {
#ifdef QT3_BUILD
        QPopupMenu* sub = new QPopupMenu(this);
        sub->setFont(font());
        parent->insertItem(title, sub);
        return sub;
#else
        QMenu* sub = parent->addMenu(title);
        sub->setFont(font());
        return sub;
#endif
    }

    static void addItem(MenuWidget34* menu, const QString& text,
                        const QObject* receiver, const char* slot) {
#ifdef QT3_BUILD
        menu->insertItem(text, receiver, slot);
#else
        menu->addAction(text, receiver, slot);
#endif
    }

    static void addSeparator(MenuWidget34* menu) {
#ifdef QT3_BUILD
        menu->insertSeparator();
#else
        menu->addSeparator();
#endif
    }

    void finalize() {
#ifdef QT3_BUILD
        int n = count();
        int gap = style().pixelMetric(
            (QStyle::PixelMetric)QStyle::PM_MenuBarItemSpacing, this);
        setMinimumWidth(sizeHint().width() + n * gap);
#else
        QStyle* ws = QStyleFactory::create("windows");
        if (ws) {
            setStyle(ws);
            setNativeMenuBar(false);
        }
        setMinimumWidth(sizeHint().width());
#endif
    }
};

#endif // EMBEDDEDMENUBAR_H
