#include "logindialog.h"
#include "translator.h"
#include "restapi.h"
#include "limelog.h"
#include "config.h"
#ifdef QT3_BUILD
#include <qtimer.h>
#else
#include <QTimer>
#endif
#include <thread>

static size_t writeCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static std::string qStrToStd(const QString& s) {
    return std::string(qToUtf8(s).data());
}

static void qComboAddItem(QComboBox* combo, const QString& text) {
#ifdef QT3_BUILD
    combo->insertItem(text);
#else
    combo->addItem(text);
#endif
}

static QString qComboText(QComboBox* combo, int index) {
#ifdef QT3_BUILD
    return combo->text(index);
#else
    return combo->itemText(index);
#endif
}

static void qComboSetCurrent(QComboBox* combo, int index) {
#ifdef QT3_BUILD
    combo->setCurrentItem(index);
#else
    combo->setCurrentIndex(index);
#endif
}

static int qComboFindText(QComboBox* combo, const QString& text) {
    for (int i = 0; i < combo->count(); ++i) {
        if (qComboText(combo, i) == text) {
            return i;
        }
    }
    return -1;
}

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    m_httpResult = -1;
    m_curlError = 0;
    m_pollTimer = new QTimer(this);

    qSetWindowTitle(this, _("login.title"));
    resize(420, 180);

    QBoxLayout* layout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 12, 12);

    QLabel* label = new QLabel(_("login.server_url"), this);
    layout->addWidget(label);

    m_urlCombo = new QComboBox(this);
    m_urlCombo->setEditable(true);
    layout->addWidget(m_urlCombo);

    QHBoxLayout* btnLayout = new QHBoxLayout();

    m_clearBtn = new QPushButton(_("login.clear_history"), this);
    btnLayout->addWidget(m_clearBtn);

    btnLayout->addStretch();

    m_connectBtn = new QPushButton(_("login.connect"), this);
    m_connectBtn->setDefault(true);
    btnLayout->addWidget(m_connectBtn);

    layout->addLayout(btnLayout);

    m_statusLabel = new QLabel("", this);
    layout->addWidget(m_statusLabel);

    loadHistory();

    QObject::connect(m_connectBtn, SIGNAL(clicked()), this, SLOT(onConnect()));
    QObject::connect(m_clearBtn, SIGNAL(clicked()), this, SLOT(onClearHistory()));
    QObject::connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(checkHttpResult()));
}

void LoginDialog::loadHistory() {
    m_urlCombo->clear();
    cJSON* root = Config::loadRoot();
    if (!root) {
        qComboAddItem(m_urlCombo, "http://localhost:8181");
        qComboSetCurrent(m_urlCombo, 0);
        return;
    }

    cJSON* hist = cJSON_GetObjectItem(root, "server_history");
    if (hist && cJSON_IsArray(hist)) {
        int count = cJSON_GetArraySize(hist);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(hist, i);
            if (item && item->valuestring) {
                qComboAddItem(m_urlCombo, qFromUtf8(item->valuestring));
            }
        }
    }

    cJSON* last = cJSON_GetObjectItem(root, "last_server");
    if (last && last->valuestring) {
        QString lastStr = qFromUtf8(last->valuestring);
        int idx = qComboFindText(m_urlCombo, lastStr);
        if (idx >= 0) {
            qComboSetCurrent(m_urlCombo, idx);
        }
    }

    cJSON_Delete(root);

    if (m_urlCombo->count() == 0) {
        qComboAddItem(m_urlCombo, "http://localhost:8181");
    }
}

void LoginDialog::saveHistory(const std::string& url) {
    cJSON* root = Config::loadRoot();
    if (!root) { root = cJSON_CreateObject(); }

    cJSON_DeleteItemFromObject(root, "last_server");
    cJSON_AddStringToObject(root, "last_server", url.c_str());

    cJSON_DeleteItemFromObject(root, "server_history");
    cJSON* hist = cJSON_CreateArray();
    cJSON_AddItemToArray(hist, cJSON_CreateString(url.c_str()));
    for (int i = 0; i < m_urlCombo->count(); ++i) {
        std::string item = qStrToStd(qComboText(m_urlCombo, i));
        if (item != url) {
            cJSON_AddItemToArray(hist, cJSON_CreateString(item.c_str()));
        }
    }
    if (cJSON_GetArraySize(hist) > 20) {
        cJSON* truncated = cJSON_CreateArray();
        for (int i = 0; i < 20 && i < cJSON_GetArraySize(hist); ++i)
            cJSON_AddItemToArray(truncated, cJSON_Duplicate(cJSON_GetArrayItem(hist, i), 1));
        cJSON_Delete(hist);
        hist = truncated;
    }
    cJSON_AddItemToObject(root, "server_history", hist);

    Config::saveRoot(root);
    cJSON_Delete(root);
}

void LoginDialog::onConnect() {
    QString urlText = qTrim(m_urlCombo->currentText());
    std::string url = qStrToStd(urlText);
    if (url.empty()) return;

    if (url.find("http://") != 0 && url.find("https://") != 0) {
        m_statusLabel->setText(_("login.invalid_url"));
        return;
    }

    if (url[url.size() - 1] == '/')
        url.resize(url.size() - 1);

    m_connectBtn->setEnabled(false);
    m_statusLabel->setText(_("login.connecting"));

    m_selectedUrl = url;
    m_httpResult = -1;
    m_curlError = 0;

    std::thread([this, url]() {
        std::string fullUrl = url + "/api/self";
        ALOG_INFO(">> GET", fullUrl);

        CURL* curl = curl_easy_init();
        std::string resp;

        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        if (res == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        }
        ALOG_INFO("<<", (int)httpCode, fullUrl);
        curl_easy_cleanup(curl);

        m_httpResult = (int)httpCode;
        m_curlError = (int)res;
    }).detach();

    m_pollTimer->start(50);
}

void LoginDialog::checkHttpResult() {
    if (m_httpResult == -1) { return; }

    m_pollTimer->stop();

    if (m_httpResult == 200) {
        m_statusLabel->setText("");
        saveHistory(m_selectedUrl);
        accept();
    } else if (m_curlError != 0) {
        m_connectBtn->setEnabled(true);
        m_statusLabel->setText(qFromUtf8(curl_easy_strerror((CURLcode)m_curlError)));
    } else if (m_httpResult > 0) {
        m_connectBtn->setEnabled(true);
        m_statusLabel->setText(QString("HTTP %1").arg(m_httpResult));
    } else {
        m_connectBtn->setEnabled(true);
        m_statusLabel->setText(_("login.timeout"));
    }
}

void LoginDialog::onClearHistory() {
    m_urlCombo->clear();
    qComboAddItem(m_urlCombo, "http://localhost:8181");
    qComboSetCurrent(m_urlCombo, 0);

    cJSON* root = Config::loadRoot();
    if (root) {
        cJSON_DeleteItemFromObject(root, "last_server");
        cJSON_DeleteItemFromObject(root, "server_history");
        Config::saveRoot(root);
        cJSON_Delete(root);
    }
}
