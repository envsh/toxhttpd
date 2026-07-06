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

Q_SIGNALS:
    void settingsRequested();
    void aboutRequested();
    void logoutRequested();

private:
    QskTextLabel* m_toastLabel = nullptr;
    QTimer* m_toastTimer = nullptr;
};

#endif
