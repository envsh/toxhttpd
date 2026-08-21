#include "systemtrayicon.h"

#include <stdio.h>
#include <qimage.h>

#ifdef QT3_BUILD
#include "trayicon.h"
#ifdef Q_WS_X11
#include <qapplication.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#else
#include <qsystemtrayicon.h>
#endif

static const int TRAY_ICON_SIZE = 22;

#ifdef QT3_BUILD

// ============ Qt3：基于 Psi TrayIcon 的适配 ============

// 事件适配器：把 Psi 的 clicked/doubleClicked 信号模型
// 转成 Qt4 的 activated(reason) 模型。右键在弹菜单前先发 Context。
// 无 Q_OBJECT（qlcomp 无 cpp 内 moc 先例），通过友元调用 fireActivated。
class SystemTrayIconAdapter : public TrayIcon {
public:
	SystemTrayIconAdapter(SystemTrayIcon* owner, const QPixmap& pm)
		: TrayIcon(pm, QString(), 0, owner), m_owner(owner) {}

protected:
	virtual void mouseMoveEvent(QMouseEvent* e) {
		e->ignore();
	}

	virtual void mousePressEvent(QMouseEvent* e) {
		switch (e->button()) {
			case RightButton:
				m_owner->fireActivated(SystemTrayIcon::Context);
				if (popup()) {
					popup()->popup(e->globalPos());
					e->accept();
					return;
				}
				break;
			case LeftButton:
				m_owner->fireActivated(SystemTrayIcon::Trigger);
				break;
			case MidButton:
				m_owner->fireActivated(SystemTrayIcon::MiddleClick);
				break;
			default:
				break;
		}
		e->ignore();
	}

	virtual void mouseReleaseEvent(QMouseEvent* e) {
		e->ignore();
	}

	virtual void mouseDoubleClickEvent(QMouseEvent* e) {
		if (e->button() == LeftButton) {
			m_owner->fireActivated(SystemTrayIcon::DoubleClick);
		}
		e->accept();
	}

private:
	SystemTrayIcon* m_owner;
};

class SystemTrayIcon::Private {
public:
	Private() : tray(0), menu(0) {}
	SystemTrayIconAdapter* tray;
	PopupMenu* menu;
};

// Psi 的托盘窗口固定 22x22 且 paintEvent 只居中不缩放，
// 大图标会被裁剪，这里统一预缩放
static QPixmap trayNormalizeIcon(const QPixmap& pm)
{
	if (pm.width() <= TRAY_ICON_SIZE && pm.height() <= TRAY_ICON_SIZE) {
		return pm;
	}
	QImage img = pm.convertToImage();
	img = img.smoothScale(TRAY_ICON_SIZE, TRAY_ICON_SIZE, QImage::ScaleMin);
	QPixmap out;
	out.convertFromImage(img);
	return out;
}

void SystemTrayIcon::initTray(const QPixmap& pm)
{
	d->tray = new SystemTrayIconAdapter(this, trayNormalizeIcon(pm));
}

void SystemTrayIcon::fireActivated(int reason)
{
	emit activated(reason);
}

void SystemTrayIcon::nativeActivated(TrayActivationReason reason)
{
	Q_UNUSED(reason)
}

SystemTrayIcon::SystemTrayIcon(const QPixmap& icon, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(icon);
}

SystemTrayIcon::SystemTrayIcon(const QString& fileName, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(QPixmap(fileName));
}

SystemTrayIcon::~SystemTrayIcon()
{
	delete d;
}

void SystemTrayIcon::setIcon(const QPixmap& icon)
{
	d->tray->setIcon(trayNormalizeIcon(icon));
}

void SystemTrayIcon::setIcon(const QString& fileName)
{
	setIcon(QPixmap(fileName));
}

QPixmap SystemTrayIcon::icon() const
{
	return d->tray->icon();
}

void SystemTrayIcon::setToolTip(const QString& tip)
{
	d->tray->setToolTip(tip);
}

QString SystemTrayIcon::toolTip() const
{
	return d->tray->toolTip();
}

void SystemTrayIcon::setVisible(bool visible)
{
	if (visible == m_visible) {
		return;
	}
	// 无托盘环境时 Psi 会在安装阶段自行拆除，保持 m_visible=false 与实际一致
	if (visible && !isSystemTrayAvailable()) {
		return;
	}
	m_visible = visible;
	if (visible) {
		d->tray->show();
	} else {
		d->tray->hide();
	}
}

bool SystemTrayIcon::isVisible() const
{
	return m_visible;
}

void SystemTrayIcon::show()
{
	setVisible(true);
}

void SystemTrayIcon::hide()
{
	setVisible(false);
}

void SystemTrayIcon::setContextMenu(PopupMenu* menu)
{
	d->menu = menu;
	d->tray->setPopup(menu);
}

PopupMenu* SystemTrayIcon::contextMenu() const
{
	return d->menu;
}

bool SystemTrayIcon::isSystemTrayAvailable()
{
#if defined(Q_WS_X11)
	Display* dsp = qt_xdisplay();
	Screen* screen = XDefaultScreenOfDisplay(dsp);
	int screen_id = XScreenNumberOfScreen(screen);
	char buf[32];
	snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen_id);
	Atom selection_atom = XInternAtom(dsp, buf, False);
	return XGetSelectionOwner(dsp, selection_atom) != None;
#else
	return false;
#endif
}

#else

// ============ Qt4：原生 QSystemTrayIcon 薄封装 ============

class SystemTrayIcon::Private {
public:
	Private() : native(0), menu(0) {}
	QSystemTrayIcon* native;
	PopupMenu* menu;
	QPixmap iconCache;
};

void SystemTrayIcon::initTray(const QPixmap& pm)
{
	d->iconCache = pm;
	d->native = new QSystemTrayIcon(QIcon(pm), this);
	connect(d->native, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(nativeActivated(QSystemTrayIcon::ActivationReason)));
}

void SystemTrayIcon::fireActivated(int reason)
{
	emit activated(reason);
}

void SystemTrayIcon::nativeActivated(TrayActivationReason reason)
{
	emit activated((int)reason);
}

SystemTrayIcon::SystemTrayIcon(const QPixmap& icon, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(icon);
}

SystemTrayIcon::SystemTrayIcon(const QString& fileName, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(QPixmap(fileName));
}

SystemTrayIcon::~SystemTrayIcon()
{
	delete d;
}

void SystemTrayIcon::setIcon(const QPixmap& icon)
{
	d->iconCache = icon;
	d->native->setIcon(QIcon(icon));
}

void SystemTrayIcon::setIcon(const QString& fileName)
{
	setIcon(QPixmap(fileName));
}

QPixmap SystemTrayIcon::icon() const
{
	return d->iconCache;
}

void SystemTrayIcon::setToolTip(const QString& tip)
{
	d->native->setToolTip(tip);
}

QString SystemTrayIcon::toolTip() const
{
	return d->native->toolTip();
}

void SystemTrayIcon::setVisible(bool visible)
{
	if (visible == m_visible) {
		return;
	}
	m_visible = visible;
	d->native->setVisible(visible);
}

bool SystemTrayIcon::isVisible() const
{
	return m_visible;
}

void SystemTrayIcon::show()
{
	setVisible(true);
}

void SystemTrayIcon::hide()
{
	setVisible(false);
}

void SystemTrayIcon::setContextMenu(PopupMenu* menu)
{
	d->menu = menu;
	d->native->setContextMenu(menu);
}

PopupMenu* SystemTrayIcon::contextMenu() const
{
	return d->menu;
}

bool SystemTrayIcon::isSystemTrayAvailable()
{
	return QSystemTrayIcon::isSystemTrayAvailable();
}

#endif
