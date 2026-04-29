#include "tox_core.h"
#include "json_util.h"
#include <tox/tox.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

struct ToxCore {
    Tox             *tox;
    EventQueue      *event_queue;
    pthread_t       iterate_thread;
    volatile bool   running;
};

static void dispatch_event(ToxCore *core, const char *event_type, const char *data)
{
    if (core->event_queue) {
        event_queue_push(core->event_queue, event_type, data);
    }
}

static void callback_connection_status(Tox *tox, Tox_Connection status, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char data[256];
    const char *status_str = (status == TOX_CONNECTION_NONE) ? "offline" : "online";
    snprintf(data, sizeof(data), "{\"connection_status\":\"%s\"}", status_str);
    dispatch_event(core, "connection_status", data);
}

static void callback_friend_request(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char pk[65] = {0};
    for (int i = 0; i < 32; i++) {
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02x", public_key[i]);
        strcat(pk, tmp);
    }
    char msg[512] = {0};
    strncpy(msg, (const char *)message, length < 511 ? length : 511);
    char data[1024];
    snprintf(data, sizeof(data), "{\"public_key\":\"%s\",\"message\":\"%s\"}", pk, msg);
    dispatch_event(core, "friend_request", data);
}

static void callback_friend_message(Tox *tox, uint32_t friend_id, Tox_Message_Type type, const uint8_t *message, size_t length, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char msg[1024] = {0};
    strncpy(msg, (const char *)message, length < 1023 ? length : 1023);
    char data[1536];
    const char *msg_type = (type == TOX_MESSAGE_TYPE_NORMAL) ? "normal" : "action";
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"message_type\":\"%s\",\"message\":\"%s\"}",
             friend_id, msg_type, msg);
    dispatch_event(core, "friend_message", data);
}

static void callback_friend_name(Tox *tox, uint32_t friend_id, const uint8_t *name, size_t length, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char n[129] = {0};
    strncpy(n, (const char *)name, length < 128 ? length : 128);
    char data[256];
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"name\":\"%s\"}", friend_id, n);
    dispatch_event(core, "friend_name", data);
}

static void callback_friend_status(Tox *tox, uint32_t friend_id, Tox_User_Status status, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    const char *status_str;
    switch (status) {
        case TOX_USER_STATUS_NONE: status_str = "none"; break;
        case TOX_USER_STATUS_AWAY: status_str = "away"; break;
        case TOX_USER_STATUS_BUSY: status_str = "busy"; break;
        default: status_str = "none";
    }
    char data[256];
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"status\":\"%s\"}", friend_id, status_str);
    dispatch_event(core, "friend_status", data);
}

static void callback_friend_status_message(Tox *tox, uint32_t friend_id, const uint8_t *message, size_t length, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char msg[1008] = {0};
    strncpy(msg, (const char *)message, length < 1007 ? length : 1007);
    char data[1280];
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"status_message\":\"%s\"}", friend_id, msg);
    dispatch_event(core, "friend_status_message", data);
}

static void callback_friend_typing(Tox *tox, uint32_t friend_id, bool is_typing, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char data[128];
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"is_typing\":%s}", friend_id, is_typing ? "true" : "false");
    dispatch_event(core, "friend_typing", data);
}

static void callback_file_recv(Tox *tox, uint32_t friend_id, uint32_t file_number, uint32_t kind, uint64_t file_size, const uint8_t *filename, size_t filename_length, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    char fn[256] = {0};
    strncpy(fn, (const char *)filename, filename_length < 255 ? filename_length : 255);
    char data[512];
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"file_id\":%u,\"kind\":%u,\"size\":%lu,\"filename\":\"%s\"}",
             friend_id, file_number, kind, (unsigned long)file_size, fn);
    dispatch_event(core, "file_recv", data);
}

static void callback_file_recv_control(Tox *tox, uint32_t friend_id, uint32_t file_number, Tox_File_Control control, void *user_data)
{
    ToxCore *core = (ToxCore *)user_data;
    (void)tox;
    const char *ctrl_str;
    switch (control) {
        case TOX_FILE_CONTROL_RESUME: ctrl_str = "resume"; break;
        case TOX_FILE_CONTROL_PAUSE: ctrl_str = "pause"; break;
        case TOX_FILE_CONTROL_CANCEL: ctrl_str = "cancel"; break;
        default: ctrl_str = "unknown";
    }
    char data[256];
    snprintf(data, sizeof(data), "{\"friend_id\":%u,\"file_id\":%u,\"control\":\"%s\"}",
             friend_id, file_number, ctrl_str);
    dispatch_event(core, "file_recv_control", data);
}

static void *iterate_thread_func(void *arg)
{
    ToxCore *core = (ToxCore *)arg;
    while (core->running) {
        tox_iterate(core->tox, core);
        uint32_t interval = tox_iteration_interval(core->tox);
        usleep(interval * 1000);
    }
    return NULL;
}

ToxCore *tox_core_init(EventQueue *event_queue)
{
    ToxCore *core = calloc(1, sizeof(ToxCore));
    if (!core) return NULL;

    core->event_queue = event_queue;

    Tox_Err_New err;
    Tox_Options *options = tox_options_new(NULL);
    if (!options) {
        free(core);
        return NULL;
    }

    tox_options_set_udp_enabled(options, true);
    tox_options_set_ipv6_enabled(options, false);

    core->tox = tox_new(options, &err);
    tox_options_free(options);

    if (!core->tox) {
        free(core);
        return NULL;
    }

    tox_callback_self_connection_status(core->tox, callback_connection_status);
    tox_callback_friend_request(core->tox, callback_friend_request);
    tox_callback_friend_message(core->tox, callback_friend_message);
    tox_callback_friend_name(core->tox, callback_friend_name);
    tox_callback_friend_status(core->tox, callback_friend_status);
    tox_callback_friend_status_message(core->tox, callback_friend_status_message);
    tox_callback_friend_typing(core->tox, callback_friend_typing);
    tox_callback_file_recv(core->tox, callback_file_recv);
    tox_callback_file_recv_control(core->tox, callback_file_recv_control);

    core->running = true;
    pthread_create(&core->iterate_thread, NULL, iterate_thread_func, core);

    return core;
}

void tox_core_destroy(ToxCore *core)
{
    if (!core) return;
    core->running = false;
    pthread_join(core->iterate_thread, NULL);
    if (core->tox) {
        tox_kill(core->tox);
    }
    free(core);
}

Tox *tox_core_get_tox(ToxCore *core)
{
    return core ? core->tox : NULL;
}

bool tox_core_bootstrap(ToxCore *core, const char *address, uint16_t port, const uint8_t *pubkey)
{
    Tox_Err_Bootstrap err;
    return tox_bootstrap(core->tox, address, port, pubkey, &err);
}

char *tox_core_get_self_address(ToxCore *core)
{
    uint8_t address[TOX_ADDRESS_SIZE];
    tox_self_get_address(core->tox, address);
    char buf[80] = {0};
    for (int i = 0; i < TOX_ADDRESS_SIZE; i++) {
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02x", address[i]);
        strcat(buf, tmp);
    }
    return strdup(buf);
}

char *tox_core_get_self_name(ToxCore *core)
{
    size_t len = tox_self_get_name_size(core->tox);
    uint8_t *name = malloc(len + 1);
    tox_self_get_name(core->tox, name);
    name[len] = '\0';
    char *result = strdup((const char *)name);
    free(name);
    return result;
}

char *tox_core_get_self_status(ToxCore *core)
{
    size_t len = tox_self_get_status_message_size(core->tox);
    uint8_t *status = malloc(len + 1);
    tox_self_get_status_message(core->tox, status);
    status[len] = '\0';
    char *result = strdup((const char *)status);
    free(status);
    return result;
}

bool tox_core_set_self_name(ToxCore *core, const char *name)
{
    Tox_Err_Set_Info err;
    size_t len = strlen(name);
    return tox_self_set_name(core->tox, (const uint8_t *)name, len, &err);
}

bool tox_core_set_self_status(ToxCore *core, const char *status)
{
    Tox_Err_Set_Info err;
    size_t len = strlen(status);
    return tox_self_set_status_message(core->tox, (const uint8_t *)status, len, &err);
}

int tox_core_get_friend_list(ToxCore *core, uint32_t *friends, size_t max_count)
{
    size_t count = tox_self_get_friend_list_size(core->tox);
    if (count > max_count) count = max_count;
    tox_self_get_friend_list(core->tox, friends);
    return (int)count;
}

char *tox_core_get_friend_name(ToxCore *core, uint32_t friend_id)
{
    size_t len = tox_friend_get_name_size(core->tox, friend_id, NULL);
    uint8_t *name = malloc(len + 1);
    bool ok = tox_friend_get_name(core->tox, friend_id, name, NULL);
    name[len] = '\0';
    char *result = ok ? strdup((const char *)name) : strdup("");
    free(name);
    return result;
}

char *tox_core_get_friend_status(ToxCore *core, uint32_t friend_id)
{
    size_t len = tox_friend_get_status_message_size(core->tox, friend_id, NULL);
    uint8_t *status = malloc(len + 1);
    bool ok = tox_friend_get_status_message(core->tox, friend_id, status, NULL);
    status[len] = '\0';
    char *result = ok ? strdup((const char *)status) : strdup("");
    free(status);
    return result;
}

bool tox_core_friend_add_norequest(ToxCore *core, const uint8_t *pubkey)
{
    Tox_Err_Friend_Add err;
    return tox_friend_add_norequest(core->tox, pubkey, &err) != UINT32_MAX;
}

bool tox_core_friend_delete(ToxCore *core, uint32_t friend_id)
{
    Tox_Err_Friend_Delete err;
    return tox_friend_delete(core->tox, friend_id, &err);
}

uint32_t tox_core_friend_send_message(ToxCore *core, uint32_t friend_id, const char *message)
{
    Tox_Err_Friend_Send_Message err;
    size_t len = strlen(message);
    return tox_friend_send_message(core->tox, friend_id, TOX_MESSAGE_TYPE_NORMAL, (const uint8_t *)message, len, &err);
}

int tox_core_get_group_list(ToxCore *core, uint32_t *groups, size_t max_count)
{
    uint32_t count = tox_group_get_group_list_size(core->tox);
    if (count > max_count) count = max_count;
    tox_group_get_group_list(core->tox, groups);
    return (int)count;
}

uint32_t tox_core_group_new(ToxCore *core)
{
    Tox_Err_Group_New err;
    Tox_Group_Number gn = tox_group_new(core->tox, TOX_GROUP_PRIVACY_STATE_PUBLIC, NULL, 0, NULL, 0, &err);
    return (err == TOX_ERR_GROUP_NEW_OK) ? gn : UINT32_MAX;
}

bool tox_core_group_join(ToxCore *core, const uint8_t *chat_id, size_t length)
{
    (void)length;
    Tox_Err_Group_Join err;
    Tox_Group_Number gn = tox_group_join(core->tox, chat_id, NULL, 0, NULL, 0, &err);
    (void)gn;
    return err == TOX_ERR_GROUP_JOIN_OK;
}

bool tox_core_group_leave(ToxCore *core, uint32_t group_id)
{
    Tox_Err_Group_Leave err;
    return tox_group_leave(core->tox, group_id, NULL, 0, &err);
}

char *tox_core_group_get_name(ToxCore *core, uint32_t group_id)
{
    size_t len = tox_group_get_name_size(core->tox, group_id, NULL);
    uint8_t *name = malloc(len + 1);
    bool ok = tox_group_get_name(core->tox, group_id, name, NULL);
    name[len] = '\0';
    char *result = ok ? strdup((const char *)name) : strdup("");
    free(name);
    return result;
}

char *tox_core_group_get_topic(ToxCore *core, uint32_t group_id)
{
    size_t len = tox_group_get_topic_size(core->tox, group_id, NULL);
    uint8_t *topic = malloc(len + 1);
    bool ok = tox_group_get_topic(core->tox, group_id, topic, NULL);
    topic[len] = '\0';
    char *result = ok ? strdup((const char *)topic) : strdup("");
    free(topic);
    return result;
}

bool tox_core_group_set_topic(ToxCore *core, uint32_t group_id, const char *topic)
{
    Tox_Err_Group_Topic_Set err;
    size_t len = strlen(topic);
    return tox_group_set_topic(core->tox, group_id, (const uint8_t *)topic, len, &err);
}

bool tox_core_group_set_self_name(ToxCore *core, uint32_t group_id, const char *name)
{
    Tox_Err_Group_Self_Name_Set err;
    size_t len = strlen(name);
    return tox_group_self_set_name(core->tox, group_id, (const uint8_t *)name, len, &err);
}

int tox_core_group_get_peers(ToxCore *core, uint32_t group_id, uint32_t *peer_ids, size_t max_count)
{
    (void)core;
    (void)group_id;
    (void)peer_ids;
    (void)max_count;
    return 0;
}

char *tox_core_group_get_peer_name(ToxCore *core, uint32_t group_id, uint32_t peer_id)
{
    size_t len = tox_group_peer_get_name_size(core->tox, group_id, peer_id, NULL);
    uint8_t *name = malloc(len + 1);
    bool ok = tox_group_peer_get_name(core->tox, group_id, peer_id, name, NULL);
    name[len] = '\0';
    char *result = ok ? strdup((const char *)name) : strdup("");
    free(name);
    return result;
}

uint32_t tox_core_group_send_message(ToxCore *core, uint32_t group_id, const char *message)
{
    Tox_Err_Group_Send_Message err;
    size_t len = strlen(message);
    return tox_group_send_message(core->tox, group_id, TOX_MESSAGE_TYPE_NORMAL, (const uint8_t *)message, len, &err);
}