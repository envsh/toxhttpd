#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "tox_core.h"
#include "push_sse.h"
#include "push_ws.h"
#include "event_queue.h"
#include "mongoose.h"

typedef struct HttpServer {
    struct mg_mgr     mgr;
    ToxCore          *tox_core;
    SSEDriver        *sse_driver;
    WSPushDriver     *ws_driver;
    EventQueue       event_queue;
    int               running;
} HttpServer;

int http_server_init(HttpServer *server, const char *port);
void http_server_destroy(HttpServer *server);
void http_server_poll(HttpServer *server, int timeout_ms);

#endif /* HTTP_SERVER_H */