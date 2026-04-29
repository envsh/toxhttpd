#include "http_server.h"
#include "json_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static void send_json(struct mg_connection *nc, const char *json)
{
    mg_http_reply(nc, 200, "Content-Type: application/json\r\n", "%s", json);
}

static void send_error(struct mg_connection *nc, int code, const char *message)
{
    mg_http_reply(nc, code, "Content-Type: application/json\r\n", "{\"error\":%d,\"message\":\"%s\"}", code, message);
}

static void handle_sse(struct mg_connection *nc, HttpServer *server)
{
    mg_printf(nc, "HTTP/1.1 200 OK\r\n");
    mg_printf(nc, "Content-Type: text/event-stream\r\n");
    mg_printf(nc, "Transfer-Encoding: chunked\r\n");
    mg_printf(nc, "Connection: keep-alive\r\n");
    mg_printf(nc, "Cache-Control: no-cache\r\n");
    mg_printf(nc, "\r\n");

    sse_driver_add_client(server->sse_driver, nc);

    while (!nc->is_closing) {
        usleep(100000);
    }

    sse_driver_remove_client(server->sse_driver, nc);
}

static void handle_api_self(struct mg_connection *nc, HttpServer *server)
{
    char *address = tox_core_get_self_address(server->tox_core);
    char *name = tox_core_get_self_name(server->tox_core);
    char *status = tox_core_get_self_status(server->tox_core);

    char *json = json_self(address, name, status, "");
    send_json(nc, json);

    free(address);
    free(name);
    free(status);
    free(json);
}

static void handle_api_self_name(struct mg_connection *nc, HttpServer *server, const char *name)
{
    bool ok = tox_core_set_self_name(server->tox_core, name);
    if (ok) {
        char *json = json_success("name set");
        send_json(nc, json);
        free(json);
    } else {
        send_error(nc, 400, "failed to set name");
    }
}

static void handle_api_self_status(struct mg_connection *nc, HttpServer *server, const char *status)
{
    bool ok = tox_core_set_self_status(server->tox_core, status);
    if (ok) {
        char *json = json_success("status set");
        send_json(nc, json);
        free(json);
    } else {
        send_error(nc, 400, "failed to set status");
    }
}

static void handle_api_bootstrap(struct mg_connection *nc, HttpServer *server, const char *address, const char *port_str, const char *pubkey_str)
{
    uint16_t port = 33445;
    if (port_str) port = atoi(port_str);

    uint8_t pubkey[32] = {0};
    if (pubkey_str && strlen(pubkey_str) >= 64) {
        for (int i = 0; i < 32; i++) {
            char tmp[3] = {pubkey_str[i*2], pubkey_str[i*2+1], 0};
            pubkey[i] = strtol(tmp, NULL, 16);
        }
    }

    bool ok = tox_core_bootstrap(server->tox_core, address, port, pubkey);
    if (ok) {
        char *json = json_success("bootstrapped");
        send_json(nc, json);
        free(json);
    } else {
        send_error(nc, 400, "bootstrap failed");
    }
}

static void handle_api_friends(struct mg_connection *nc, HttpServer *server)
{
    uint32_t friends[256];
    int count = tox_core_get_friend_list(server->tox_core, friends, 256);

    char *json = json_friend_list(friends, count);
    send_json(nc, json);
    free(json);
}

static void handle_api_friends_add(struct mg_connection *nc, HttpServer *server, const char *pubkey_str)
{
    if (!pubkey_str || strlen(pubkey_str) < 64) {
        send_error(nc, 400, "invalid public key");
        return;
    }

    uint8_t pubkey[32];
    for (int i = 0; i < 32; i++) {
        char tmp[3] = {pubkey_str[i*2], pubkey_str[i*2+1], 0};
        pubkey[i] = strtol(tmp, NULL, 16);
    }

    bool ok = tox_core_friend_add_norequest(server->tox_core, pubkey);
    if (ok) {
        char *json = json_success("friend added");
        send_json(nc, json);
        free(json);
    } else {
        send_error(nc, 400, "failed to add friend");
    }
}

static void handle_api_messages(struct mg_connection *nc, HttpServer *server, const char *friend_id_str, const char *message)
{
    if (!friend_id_str || !message) {
        send_error(nc, 400, "missing parameters");
        return;
    }

    uint32_t friend_id = atoi(friend_id_str);
    uint32_t msg_id = tox_core_friend_send_message(server->tox_core, friend_id, message);

    char *json = json_message_sent(msg_id);
    send_json(nc, json);
    free(json);
}

static void handle_api_groups(struct mg_connection *nc, HttpServer *server)
{
    uint32_t groups[64];
    int count = tox_core_get_group_list(server->tox_core, groups, 64);

    char *json = json_group_list(groups, count);
    send_json(nc, json);
    free(json);
}

static void handle_api_groups_create(struct mg_connection *nc, HttpServer *server)
{
    uint32_t group_id = tox_core_group_new(server->tox_core);
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"group_id\":%u}", group_id);
    mg_http_reply(nc, 200, "Content-Type: application/json\r\n", "%s", buf);
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    HttpServer *server = (HttpServer *)nc->mgr;
    struct mg_http_message *hm;

    switch (ev) {
        case MG_EV_HTTP_MSG: {
            hm = (struct mg_http_message *)ev_data;

            if (mg_strcmp(hm->method, mg_str("GET")) == 0) {
                if (mg_strcmp(hm->uri, mg_str("/api/self")) == 0) {
                    handle_api_self(nc, server);
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/friends")) == 0) {
                    handle_api_friends(nc, server);
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/groups")) == 0) {
                    handle_api_groups(nc, server);
                }
                else if (mg_strcmp(hm->uri, mg_str("/events/sse")) == 0) {
                    handle_sse(nc, server);
                }
                else if (mg_strcmp(hm->uri, mg_str("/events/ws")) == 0) {
                    mg_ws_upgrade(nc, hm, NULL);
                    ws_driver_add_client(server->ws_driver, nc);
                }
                else {
                    mg_http_reply(nc, 404, "Content-Type: application/json\r\n", "{\"error\":404,\"message\":\"not found\"}");
                }
            }
            else if (mg_strcmp(hm->method, mg_str("POST")) == 0) {
                if (mg_strcmp(hm->uri, mg_str("/api/self/name")) == 0) {
                    char name[128] = {0};
                    mg_http_get_var(&hm->body, "name", name, sizeof(name));
                    if (name[0]) handle_api_self_name(nc, server, name);
                    else send_error(nc, 400, "missing name");
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/self/status")) == 0) {
                    char status[1024] = {0};
                    mg_http_get_var(&hm->body, "status", status, sizeof(status));
                    if (status[0]) handle_api_self_status(nc, server, status);
                    else send_error(nc, 400, "missing status");
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/bootstrap")) == 0) {
                    char address[256] = {0};
                    char port[16] = {0};
                    char pubkey[64] = {0};
                    mg_http_get_var(&hm->body, "address", address, sizeof(address));
                    mg_http_get_var(&hm->body, "port", port, sizeof(port));
                    mg_http_get_var(&hm->body, "public_key", pubkey, sizeof(pubkey));
                    if (address[0]) handle_api_bootstrap(nc, server, address, port, pubkey);
                    else send_error(nc, 400, "missing address");
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/friends")) == 0) {
                    char pubkey[64] = {0};
                    mg_http_get_var(&hm->body, "public_key", pubkey, sizeof(pubkey));
                    if (pubkey[0]) handle_api_friends_add(nc, server, pubkey);
                    else send_error(nc, 400, "missing public_key");
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/messages")) == 0) {
                    char friend_id[32] = {0};
                    char message[1024] = {0};
                    mg_http_get_var(&hm->body, "friend_id", friend_id, sizeof(friend_id));
                    mg_http_get_var(&hm->body, "message", message, sizeof(message));
                    handle_api_messages(nc, server, friend_id, message);
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/groups")) == 0) {
                    handle_api_groups_create(nc, server);
                }
                else {
                    mg_http_reply(nc, 404, "Content-Type: application/json\r\n", "{\"error\":404,\"message\":\"not found\"}");
                }
            }
            else {
                mg_http_reply(nc, 405, "Content-Type: application/json\r\n", "{\"error\":405,\"message\":\"method not allowed\"}");
            }
            break;
        }
        case MG_EV_WS_MSG: {
            break;
        }
        case MG_EV_CLOSE: {
            ws_driver_remove_client(server->ws_driver, nc);
            break;
        }
    }
}

static void *dispatch_thread_func(void *arg)
{
    HttpServer *server = (HttpServer *)arg;
    EventQueue *queue = &server->event_queue;

    while (server->running) {
        Event event;
        int ret = event_queue_pop(queue, &event);
        if (ret == 0) {
            sse_driver_broadcast(server->sse_driver, &event);
            ws_driver_broadcast(server->ws_driver, &event);
        }
    }
    return NULL;
}

int http_server_init(HttpServer *server, const char *port)
{
    memset(server, 0, sizeof(*server));

    event_queue_init(&server->event_queue);

    server->tox_core = tox_core_init(&server->event_queue);
    if (!server->tox_core) {
        return -1;
    }

    server->sse_driver = sse_driver_init();
    server->ws_driver = ws_driver_init();

    server->running = 1;

    pthread_t dispatch_thread;
    pthread_create(&dispatch_thread, NULL, dispatch_thread_func, server);

    mg_mgr_init(&server->mgr);
    mg_http_listen(&server->mgr, port, ev_handler, NULL);

    return 0;
}

void http_server_destroy(HttpServer *server)
{
    server->running = 0;
    mg_mgr_free(&server->mgr);
    tox_core_destroy(server->tox_core);
    sse_driver_destroy(server->sse_driver);
    ws_driver_destroy(server->ws_driver);
    event_queue_destroy(&server->event_queue);
}

void http_server_poll(HttpServer *server, int timeout_ms)
{
    mg_mgr_poll(&server->mgr, timeout_ms);
}