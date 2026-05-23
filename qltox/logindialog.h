#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "compat34.h"
#include <string>
#include <curl/curl.h>
#include <qdialog.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qtimer.h>

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    std::string selectedUrl() const { return m_selectedUrl; }
    static QString configValue(const QString& key);
    static bool setConfigValue(const QString& key, const QString& value);
private slots:
    void onConnect();
    void onClearHistory();
    void checkHttpResult();
private:
    QComboBox* m_urlCombo;
    QPushButton* m_connectBtn;
    QPushButton* m_clearBtn;
    QLabel* m_statusLabel;
    std::string m_selectedUrl;
    volatile int m_httpResult;
    volatile int m_curlError;
    QTimer* m_pollTimer;

    void loadHistory();
    void saveHistory(const std::string& url);
};

#endif
