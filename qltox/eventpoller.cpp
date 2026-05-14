#include "eventpoller.h"
#include "apilog.h"
#include <unistd.h>

EventPoller* EventPoller::s_instance = nullptr;

static size_t writeCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static size_t headerCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userp);
    std::string line(static_cast<char*>(contents), total);
    size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0) return total;
    std::string name = line.substr(0, colon);
    size_t vStart = colon + 1;
    while (vStart < line.size() && (line[vStart] == ' ' || line[vStart] == '\t')) vStart++;
    size_t vEnd = line.size();
    while (vEnd > vStart && (line[vEnd - 1] == '\r' || line[vEnd - 1] == '\n')) vEnd--;
    if (vEnd <= vStart) return total;
    for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    (*headers)[name] = line.substr(vStart, vEnd - vStart);
    return total;
}

EventPoller::EventPoller()
    : QThread(), running(true), multi(nullptr) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    multi = curl_multi_init();
    s_instance = this;
}

void EventPoller::start() {
    if (!s_instance) {
        auto* ep = new EventPoller();
        ep->QThread::start();
    }
}

void EventPoller::stop() {
    if (!s_instance) return;
    s_instance->running = false;
    s_instance->wait();
    if (s_instance->multi) {
        curl_multi_cleanup(s_instance->multi);
        s_instance->multi = nullptr;
    }
    delete s_instance;
    s_instance = nullptr;
}

void EventPoller::addRequest(const std::string& url,
                              const std::string& method,
                              const std::string& data,
                              void (*done)(int, const std::string&,
                                          const std::map<std::string, std::string>*,
                                          void*),
                              void* udata, int timeoutSec) {
    if (!s_instance || !s_instance->multi) return;

    CURL* easy = curl_easy_init();
    if (!easy) return;

    auto* ctx = new HttpCtx{std::string(), std::map<std::string, std::string>(), done, udata};

    curl_easy_setopt(easy, CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy, CURLOPT_PRIVATE, ctx);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &ctx->body);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &ctx->headers);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, (long)timeoutSec);
    curl_easy_setopt(easy, CURLOPT_TCP_KEEPALIVE, 1L);

    if (method == "POST") {
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)data.size());
    }

    s_instance->multiMutex.lock();
    curl_multi_add_handle(s_instance->multi, easy);
    s_instance->multiMutex.unlock();
}

void EventPoller::run() {
    while (running) {
        int numfds;
        curl_multi_wait(multi, NULL, 0, 50, &numfds);
        int r;
        while (curl_multi_perform(multi, &r) == CURLM_CALL_MULTI_PERFORM);

        CURLMsg* msg;
        int left;
        while ((msg = curl_multi_info_read(multi, &left))) {
            if (msg->msg == CURLMSG_DONE) {
                HttpCtx* ctx;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
                long httpCode = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &httpCode);
                ctx->done(httpCode, ctx->body, &ctx->headers, ctx->udata);
                delete ctx;
                curl_multi_remove_handle(multi, msg->easy_handle);
                curl_easy_cleanup(msg->easy_handle);
            }
        }
    }
}
