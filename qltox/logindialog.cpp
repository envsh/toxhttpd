#include "logindialog.h"
#include "translator.h"
#include "restapi.h"
#include "cJSON.h"
#ifdef QT3_BUILD
#include <qtimer.h>
#else
#include <QTimer>
#endif
#include <thread>
#include <fstream>
#include <sstream>
#include <cstdlib>

static size_t writeCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static std::string qStrToStd(const QString& s) {
    return std::string(qToUtf8(s).data());
}

static QString configDir() {
#ifdef QT3_BUILD
    return QDir::homeDirPath() + "/.config/toxhttpd";
#else
    return QDir::homePath() + "/.config/toxhttpd";
#endif
}

static std::string configFilePath() {
    return qStrToStd(configDir()) + "/servers";
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
        if (qComboText(combo, i) == text)
            return i;
    }
    return -1;
}

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    m_httpResult = -1;
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
    std::string path = configFilePath();
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        qComboAddItem(m_urlCombo, "http://localhost:8181");
        qComboSetCurrent(m_urlCombo, 0);
        return;
    }

    std::stringstream ss;
    ss << ifs.rdbuf();
    std::string content = ss.str();

    cJSON* root = cJSON_Parse(content.c_str());
    if (!root) {
        qComboAddItem(m_urlCombo, "http://localhost:8181");
        qComboSetCurrent(m_urlCombo, 0);
        return;
    }

    cJSON* hist = cJSON_GetObjectItem(root, "history");
    if (hist && cJSON_IsArray(hist)) {
        int count = cJSON_GetArraySize(hist);
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(hist, i);
            if (item && item->valuestring)
                qComboAddItem(m_urlCombo, qFromUtf8(item->valuestring));
        }
    }

    cJSON* last = cJSON_GetObjectItem(root, "last");
    if (last && last->valuestring) {
        QString lastStr = qFromUtf8(last->valuestring);
        int idx = qComboFindText(m_urlCombo, lastStr);
        if (idx >= 0)
            qComboSetCurrent(m_urlCombo, idx);
    }

    cJSON_Delete(root);

    if (m_urlCombo->count() == 0)
        qComboAddItem(m_urlCombo, "http://localhost:8181");
}

void LoginDialog::saveHistory(const std::string& url) {
    std::string path = configFilePath();
    std::string dir = path.substr(0, path.rfind('/'));
    std::string mkdirCmd = "mkdir -p " + dir;
    system(mkdirCmd.c_str());

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "last", url.c_str());

    cJSON* hist = cJSON_CreateArray();
    cJSON_AddItemToArray(hist, cJSON_CreateString(url.c_str()));
    for (int i = 0; i < m_urlCombo->count(); ++i) {
        std::string item = qStrToStd(qComboText(m_urlCombo, i));
        if (item != url)
            cJSON_AddItemToArray(hist, cJSON_CreateString(item.c_str()));
    }
    if (cJSON_GetArraySize(hist) > 20) {
        cJSON* truncated = cJSON_CreateArray();
        for (int i = 0; i < 20 && i < cJSON_GetArraySize(hist); ++i)
            cJSON_AddItemToArray(truncated, cJSON_Duplicate(cJSON_GetArrayItem(hist, i), 1));
        cJSON_Delete(hist);
        hist = truncated;
    }
    cJSON_AddItemToObject(root, "history", hist);

    char* jsonStr = cJSON_Print(root);
    std::ofstream ofs(path);
    if (ofs.is_open())
        ofs << jsonStr;
    free(jsonStr);
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

    std::thread([this, url]() {
        CURL* curl = curl_easy_init();
        std::string resp;

        std::string fullUrl = url + "/api/self";
        curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        if (res == CURLE_OK)
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);

        m_httpResult = (int)httpCode;
    }).detach();

    m_pollTimer->start(50);
}

void LoginDialog::checkHttpResult() {
    if (m_httpResult == -1) return;

    m_pollTimer->stop();

    if (m_httpResult == 200) {
        m_statusLabel->setText("");
        saveHistory(m_selectedUrl);
        accept();
    } else {
        m_connectBtn->setEnabled(true);
        m_statusLabel->setText(m_httpResult == 0
            ? _("login.timeout")
            : _("login.failed"));
    }
}

void LoginDialog::onClearHistory() {
    m_urlCombo->clear();
    qComboAddItem(m_urlCombo, "http://localhost:8181");
    qComboSetCurrent(m_urlCombo, 0);

    std::string path = configFilePath();
    std::remove(path.c_str());
}
