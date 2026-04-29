#include "http_server.h"
#include "json_util.h"
#include "bootstrap.h"
#include "log.h"
#include <tox/tox.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

static HttpServer *global_server = NULL;

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
    Tox *tox = tox_core_get_tox(server->tox_core);
    Tox_Connection conn = tox_self_get_connection_status(tox);
    const char *conn_str = (conn == TOX_CONNECTION_NONE) ? "offline" : (conn == TOX_CONNECTION_TCP ? "tcp" : "udp");

    char *json = json_self(address, name, status, "", conn_str);
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
    uint8_t pubkey[32] = {0};
    char addr[256] = {0};
    uint16_t port = 33445;
    
    // Use random if no address provided
    if (address == NULL || address[0] == '\0') {
        bootstrap_random(pubkey, &port, addr);
    } else {
        // User provided address
        strncpy(addr, address, sizeof(addr) - 1);
        port = (port_str && port_str[0]) ? (uint16_t)atoi(port_str) : 33445;
    }
    
    bool ok = tox_core_bootstrap(server->tox_core, addr, port, pubkey);
    char *json = ok ? json_success("bootstrapped") : json_success("bootstrap initiated");
    send_json(nc, json);
    free(json);
}

static void handle_api_friends(struct mg_connection *nc, HttpServer *server)
{
    uint32_t friends[256];
    int count = tox_core_get_friend_list(server->tox_core, friends, 256);

    char *json = json_friend_list(friends, count);
    send_json(nc, json);
    free(json);
}

static void handle_api_friend_get(struct mg_connection *nc, HttpServer *server, uint32_t friend_id)
{
    char *name = tox_core_get_friend_name(server->tox_core, friend_id);
    char *status = tox_core_get_friend_status(server->tox_core, friend_id);
    int conn = tox_core_get_friend_connection_status(server->tox_core, friend_id);
    const char *conn_str = (conn == TOX_CONNECTION_NONE) ? "offline" : (conn == TOX_CONNECTION_TCP ? "tcp" : "udp");

    uint8_t pubkey[32];
    bool has_pk = tox_core_get_friend_public_key(server->tox_core, friend_id, pubkey);

    Tox *tox = tox_core_get_tox(server->tox_core);
    int self_conn = tox_self_get_connection_status(tox);
    const char *self_conn_str = (self_conn == TOX_CONNECTION_NONE) ? "offline" : (self_conn == TOX_CONNECTION_TCP ? "tcp" : "udp");
    
    uint8_t address[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, address);
    char address_hex[TOX_ADDRESS_SIZE * 2 + 1];
    for (int i = 0; i < TOX_ADDRESS_SIZE; i++) {
        sprintf(address_hex + i*2, "%02x", address[i]);
    }

    char *json = json_friend_info(friend_id, name, status, 0, has_pk ? pubkey : NULL, conn_str, self_conn_str, address_hex);
    send_json(nc, json);
    free(json);
    free(name);
    free(status);
}

static void parse_hex_to_pubkey(const char *hex, uint8_t *pubkey, size_t len)
{
    memset(pubkey, 0, len);
    for (size_t i = 0; hex[i] && hex[i+1] && i < len * 2; i += 2) {
        char tmp[3] = {hex[i], hex[i+1], 0};
        pubkey[i/2] = strtol(tmp, NULL, 16);
    }
}

static void handle_api_friends_add(struct mg_connection *nc, HttpServer *server, const char *pubkey_str)
{
    if (!pubkey_str || strlen(pubkey_str) < 64) {
        send_error(nc, 400, "invalid public key");
        return;
    }

    size_t len = strlen(pubkey_str);
    fprintf(stderr, "DEBUG: pubkey_str len=%zu\n", len);
    LOGD("pubkey_str len=%zu", len);

    Tox *tox = tox_core_get_tox(server->tox_core);
    Tox_Err_Friend_Add err;
    uint32_t fn;
    
    if (len < 76) {
        uint8_t pubkey[32];
        parse_hex_to_pubkey(pubkey_str, pubkey, 32);
        fn = tox_friend_add_norequest(tox, pubkey, &err);
        LOGD("friend_add (norequest): fn=%u err=%d (%s)", fn, err, tox_err_friend_add_to_string(err));
    } else {
        uint8_t address[TOX_ADDRESS_SIZE];
        parse_hex_to_pubkey(pubkey_str, address, TOX_ADDRESS_SIZE);
        fn = tox_friend_add(tox, address, (const uint8_t *)"Hello!", 6, &err);
        LOGD("friend_add (request): fn=%u err=%d (%s)", fn, err, tox_err_friend_add_to_string(err));
    }
    
    if (fn != UINT32_MAX) {
        char *json = json_success("friend added");
        send_json(nc, json);
        free(json);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "failed to add friend: %s (%d)", 
            tox_err_friend_add_to_string(err), err);
        send_error(nc, 400, msg);
    }
}

static void handle_api_messages(struct mg_connection *nc, HttpServer *server, const char *friend_id_str, const char *message)
{
    if (!friend_id_str || !message) {
        send_error(nc, 400, "missing parameters");
        return;
    }

    uint32_t friend_id = atoi(friend_id_str);
    Tox *tox = tox_core_get_tox(server->tox_core);
    Tox_Err_Friend_Send_Message err;
    uint32_t msg_id = tox_friend_send_message(tox, friend_id, TOX_MESSAGE_TYPE_NORMAL, 
        (const uint8_t *)message, strlen(message), &err);
    
    LOGD("send_message: friend_id=%u msg_id=%u err=%d", friend_id, msg_id, err);
    
    if (err == TOX_ERR_FRIEND_SEND_MESSAGE_OK) {
        char *json = json_message_sent(msg_id);
        send_json(nc, json);
        free(json);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg), "failed to send: %s (%d)", 
            tox_err_friend_send_message_to_string(err), err);
        send_error(nc, 400, msg);
    }
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

static void handle_api_group_message(struct mg_connection *nc, HttpServer *server, const char *group_id_str, const char *message)
{
    if (!group_id_str || !message) {
        send_error(nc, 400, "missing parameters");
        return;
    }
    
    uint32_t group_id = atoi(group_id_str);
    uint32_t msg_id = tox_core_group_send_message(server->tox_core, group_id, message);
    
    LOGD("group_send: group_id=%u msg_id=%u", group_id, msg_id);
    
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"message_id\":%u}", msg_id);
    send_json(nc, buf);
}

static void handle_api_conferences(struct mg_connection *nc, HttpServer *server)
{
    uint32_t conf_list[64];
    int count = tox_core_get_conference_list(server->tox_core, conf_list, 64);

    char buf[512] = {"{\"conferences\":["};
    for (int i = 0; i < count; i++) {
        if (i > 0) strcat(buf, ",");
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%u", conf_list[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "]}");
    send_json(nc, buf);
}

static void handle_api_conferences_create(struct mg_connection *nc, HttpServer *server)
{
    uint32_t conf_id = tox_core_conference_new(server->tox_core);
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"conference_id\":%u}", conf_id);
    send_json(nc, buf);
}

static void handle_api_conference_message(struct mg_connection *nc, HttpServer *server, const char *conf_id_str, const char *message)
{
    if (!conf_id_str || !message) {
        send_error(nc, 400, "missing parameters");
        return;
    }
    
    uint32_t conf_id = atoi(conf_id_str);
    uint32_t msg_id = tox_core_conference_send_message(server->tox_core, conf_id, message);
    
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"message_id\":%u}", msg_id);
    send_json(nc, buf);
}

static void handle_api_conference_invite(struct mg_connection *nc, HttpServer *server, uint32_t friend_id, uint32_t conf_id)
{
    bool ok = tox_core_conference_invite(server->tox_core, friend_id, conf_id);
    LOGI("conference_invite friend_id=%u conf_id=%u result=%s", friend_id, conf_id, ok ? "ok" : "fail");
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"success\":%s}", ok ? "true" : "false");
    send_json(nc, buf);
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    HttpServer *server = global_server;
    if (!server || !server->running) {
        return;
    }
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
                else if (mg_strcmp(hm->uri, mg_str("/api/conferences")) == 0) {
                    handle_api_conferences(nc, server);
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
                    mg_http_get_var(&hm->body, "status_message", status, sizeof(status));
                    if (status[0]) handle_api_self_status(nc, server, status);
                    else send_error(nc, 400, "missing status_message");
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/bootstrap")) == 0) {
                    char address[256] = {0};
                    char port[16] = {0};
                    char pubkey[64] = {0};
                    mg_http_get_var(&hm->body, "address", address, sizeof(address));
                    mg_http_get_var(&hm->body, "port", port, sizeof(port));
                    mg_http_get_var(&hm->body, "public_key", pubkey, sizeof(pubkey));
                    // Empty or missing address triggers random bootstrap
                    handle_api_bootstrap(nc, server, address[0] ? address : NULL, port, pubkey);
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/friends")) == 0) {
                    char pubkey[128] = {0};
                    mg_http_get_var(&hm->body, "public_key", pubkey, sizeof(pubkey));
                    if (pubkey[0]) handle_api_friends_add(nc, server, pubkey);
                    else send_error(nc, 400, "missing public_key");
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/friend")) == 0) {
                    char friend_id[32] = {0};
                    mg_http_get_var(&hm->body, "friend_id", friend_id, sizeof(friend_id));
                    if (friend_id[0]) {
                        handle_api_friend_get(nc, server, atoi(friend_id));
                    } else {
                        send_error(nc, 400, "missing friend_id");
                    }
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
                else if (mg_strcmp(hm->uri, mg_str("/api/group_messages")) == 0) {
                    char group_id[32] = {0};
                    char message[1024] = {0};
                    mg_http_get_var(&hm->body, "group_id", group_id, sizeof(group_id));
                    mg_http_get_var(&hm->body, "message", message, sizeof(message));
                    handle_api_group_message(nc, server, group_id, message);
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/conferences")) == 0) {
                    handle_api_conferences_create(nc, server);
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/conference_invite")) == 0) {
                    char friend_id[32] = {0};
                    char conf_id[32] = {0};
                    mg_http_get_var(&hm->body, "friend_id", friend_id, sizeof(friend_id));
                    mg_http_get_var(&hm->body, "conference_id", conf_id, sizeof(conf_id));
                    if (friend_id[0] && conf_id[0]) {
                        handle_api_conference_invite(nc, server, atoi(friend_id), atoi(conf_id));
                    } else {
                        send_error(nc, 400, "missing friend_id or conference_id");
                    }
                }
                else if (mg_strcmp(hm->uri, mg_str("/api/conference_messages")) == 0) {
                    char conf_id[32] = {0};
                    char message[1024] = {0};
                    mg_http_get_var(&hm->body, "conference_id", conf_id, sizeof(conf_id));
                    mg_http_get_var(&hm->body, "message", message, sizeof(message));
                    handle_api_conference_message(nc, server, conf_id, message);
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
    
    // Load bootstrap nodes
    bootstrap_load("bootstrap.json");

    server->tox_core = tox_core_init(&server->event_queue);
    if (!server->tox_core) {
        LOGE("Failed to init tox_core");
        return -1;
    }

    server->sse_driver = sse_driver_init();
    server->ws_driver = ws_driver_init();

    server->running = 1;

    global_server = server;

    pthread_t dispatch_thread;
    pthread_create(&dispatch_thread, NULL, dispatch_thread_func, server);

    mg_mgr_init(&server->mgr);
    
    char url[64];
    snprintf(url, sizeof(url), "http://0.0.0.0:%s", port);
    
    if (!mg_http_listen(&server->mgr, url, ev_handler, server)) {
        LOGE("Failed to listen on %s", url);
        return -1;
    }
    
    LOGI("Server listening on http://localhost:%s", port);
    
    Tox *tox = tox_core_get_tox(server->tox_core);
    uint8_t address[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, address);
    LOGI("My Tox ID: ");
    for (int i = 0; i < TOX_ADDRESS_SIZE; i++) {
        fprintf(stderr, "%02x", address[i]);
    }
    fprintf(stderr, "\n");
    
    return 0;
}

void http_server_destroy(HttpServer *server)
{
    server->running = 0;
    global_server = NULL;
    tox_core_destroy(server->tox_core);
    mg_mgr_free(&server->mgr);
    sse_driver_destroy(server->sse_driver);
    ws_driver_destroy(server->ws_driver);
    event_queue_destroy(&server->event_queue);
}

void http_server_poll(HttpServer *server, int timeout_ms)
{
    mg_mgr_poll(&server->mgr, timeout_ms);
}

void http_server_stop(HttpServer *server)
{
    server->running = 0;
    if (server->mgr.conns) {
        for (struct mg_connection *c = server->mgr.conns; c; c = c->next) {
            c->is_closing = 1;
        }
    }
}