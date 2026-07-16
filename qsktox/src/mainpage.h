#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"

class QskTextLabel;
class QTimer;

class MainPage : public Page
{
    Q_OBJECT
public:
    MainPage(QQuickItem* parent = nullptr);

    void showToast(const QString& msg, int durationMs = 2000);
    void handleShareIntent(const QString& action, const QString& mimeType,
                           const QString& text, const QString& urisJson);
    bool keepScreenOn() const { return m_keepScreenOn; }
    void setKeepScreenOn(bool on);

Q_SIGNALS:
    void keepScreenOnChanged(bool on);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onNewIntent(const QVariantMap& launchArgs) override;

private:
    QskTextLabel* m_toastLabel = nullptr;
    QTimer* m_toastTimer = nullptr;
    bool m_keepScreenOn = true;
};

void registerMainPage(MainPage* page);

#endif
