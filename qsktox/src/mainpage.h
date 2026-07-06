#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include <QskControl.h>

class QskTextLabel;
class QTimer;

class MainPage : public QskControl
{
    Q_OBJECT
public:
    MainPage(QQuickItem* parent = nullptr);

    void showToast(const QString& msg, int durationMs = 2000);
    bool keepScreenOn() const { return m_keepScreenOn; }
    void setKeepScreenOn(bool on);

Q_SIGNALS:
    void settingsRequested();
    void aboutRequested();
    void logoutRequested();
    void keepScreenOnChanged(bool on);

private:
    QskTextLabel* m_toastLabel = nullptr;
    QTimer* m_toastTimer = nullptr;
    bool m_keepScreenOn = true;
};

#endif
