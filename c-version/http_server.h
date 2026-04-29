#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "tox_core.h"
#include "push_sse.h"
#include "push_ws.h"
#include "event_queue.h"
#include "mongoose.h"

typedef struct PendingRequest {
    struct mg_connection *nc;
    uint64_t              after;
    uint64_t              timestamp;  // 用于超时检测
    struct PendingRequest *next;
} PendingRequest;

typedef struct HttpServer {
    struct mg_mgr     mgr;
    ToxCore          *tox_core;
    SSEDriver        *sse_driver;
    WSPushDriver     *ws_driver;
    EventQueue       event_queue;
    int               running;
    uint64_t          last_event_id;  // 用于跟踪已处理的事件
    PendingRequest   *pending_requests;  // 挂起的长轮询请求
} HttpServer;

int http_server_init(HttpServer *server, const char *port);
void http_server_destroy(HttpServer *server);
void http_server_poll(HttpServer *server, int timeout_ms);
void http_server_stop(HttpServer *server);

#endif /* HTTP_SERVER_H */