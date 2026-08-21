#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <qobject.h>
#include <qstring.h>
#include <qpixmap.h>

// PopupMenu/TrayActivationReason 本地 typedef（不改 compat34.h），
// 参照 EmbeddedMenuBar.h 的 MenuWidget34 先例；#ifdef 均在类体之外（moc 安全）
#ifdef QT3_BUILD
#include <qpopupmenu.h>
typedef QPopupMenu PopupMenu;
typedef int TrayActivationReason;
#else
#include <qmenu.h>
#include <qsystemtrayicon.h>
typedef QMenu PopupMenu;
typedef QSystemTrayIcon::ActivationReason TrayActivationReason;
#endif

/*
 * SystemTrayIcon - 系统托盘图标（Qt3/Qt4 双兼容）
 *
 * API 对齐 Qt4 QSystemTrayIcon 范式。
 * Qt3 分支基于 Psi 0.10 TrayIcon（trayicon*.{h,cpp}，LGPL 2.1）适配。
 * showMessage 气泡 v1 未实现。
 */
class SystemTrayIcon : public QObject {
	Q_OBJECT

public:
	// 数值与 Qt4 QSystemTrayIcon::ActivationReason 一致
	enum ActivationReason {
		Unknown = 0,
		Context = 1,
		DoubleClick = 2,
		Trigger = 3,
		MiddleClick = 4
	};

	SystemTrayIcon(const QPixmap& icon, QObject* parent = 0);
	SystemTrayIcon(const QString& fileName, QObject* parent = 0);
	~SystemTrayIcon();

	void setIcon(const QPixmap& icon);
	void setIcon(const QString& fileName);
	QPixmap icon() const;

	void setToolTip(const QString& tip);
	QString toolTip() const;

	void setVisible(bool visible);
	bool isVisible() const;
	void show();
	void hide();

	void setContextMenu(PopupMenu* menu);
	PopupMenu* contextMenu() const;

	static bool isSystemTrayAvailable();

signals:
	void activated(int reason);

private slots:
	// Qt4 分支的原生信号中继；Qt3 分支为空实现（不被连接）
	void nativeActivated(TrayActivationReason reason);

private:
	friend class SystemTrayIconAdapter;
	void fireActivated(int reason);
	void initTray(const QPixmap& pm);

	class Private;
	Private* d;
	bool m_visible;
};

#endif  // SYSTEMTRAYICON_H
