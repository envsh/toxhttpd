#include "json_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static char *strdup_safe(const char *s)
{
    if (!s) return strdup("");
    return strdup(s);
}

char *json_error(int code, const char *message)
{
    char *msg = strdup_safe(message);
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"error\":%d,\"message\":\"%s\"}", code, msg);
    free(msg);
    return strdup(buf);
}

char *json_self(const char *address, const char *name, const char *status, const char *status_emoji, const char *connection_status)
{
    char *n = strdup_safe(name);
    char *s = strdup_safe(status);
    char *e = strdup_safe(status_emoji);
    char *addr = strdup_safe(address);
    char *conn = strdup_safe(connection_status);

    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"address\":\"%s\",\"name\":\"%s\",\"status_message\":\"%s\",\"status_emoji\":\"%s\",\"connection_status\":\"%s\"}",
        addr, n, s, e, conn);

    free(n); free(s); free(e); free(addr); free(conn);
    return strdup(buf);
}

char *json_friend_list(const uint32_t *friend_ids, size_t count)
{
    char *buf = malloc(256 + count * 16);
    if (!buf) return NULL;

    strcpy(buf, "{\"friends\":[");
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat(buf, ",");
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%u", friend_ids[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "]}");
    return buf;
}

char *json_friend_info(uint32_t friend_id, const char *name, const char *status, int status_enum, const uint8_t *pubkey, const char *connection_status, const char *self_connection_status)
{
    char *n = strdup_safe(name);
    char *s = strdup_safe(status);
    char buf[1024];

    char pk[64 + 1] = {0};
    for (int i = 0; i < 32 && pubkey; i++) {
        char tmp[4];
        snprintf(tmp, sizeof(tmp), "%02x", pubkey[i]);
        strcat(pk, tmp);
    }

    const char *status_str;
    switch (status_enum) {
        case 0: status_str = "none"; break;
        case 1: status_str = "away"; break;
        case 2: status_str = "busy"; break;
        default: status_str = "none";
    }

    const char *conn = connection_status ? connection_status : "offline";
    const char *self_conn = self_connection_status ? self_connection_status : "offline";
    snprintf(buf, sizeof(buf),
        "{\"friend_id\":%u,\"name\":\"%s\",\"status_message\":\"%s\",\"status\":\"%s\",\"connection_status\":\"%s\",\"public_key\":\"%s\",\"self_connection_status\":\"%s\"}",
        friend_id, n, s, status_str, conn, pk, self_conn);

    free(n); free(s);
    return strdup(buf);
}

char *json_group_list(const uint32_t *group_ids, size_t count)
{
    char *buf = malloc(256 + count * 16);
    if (!buf) return NULL;

    strcpy(buf, "{\"groups\":[");
    for (size_t i = 0; i < count; i++) {
        if (i > 0) strcat(buf, ",");
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%u", group_ids[i]);
        strcat(buf, tmp);
    }
    strcat(buf, "]}");
    return buf;
}

char *json_group_info(uint32_t group_id, const char *name, const char *topic, const char *privacy_state, const char *voice_state)
{
    char *n = strdup_safe(name);
    char *t = strdup_safe(topic);
    char buf[1024];

    snprintf(buf, sizeof(buf),
        "{\"group_id\":%u,\"name\":\"%s\",\"topic\":\"%s\",\"privacy_state\":\"%s\",\"voice_state\":\"%s\"}",
        group_id, n, t, privacy_state ? privacy_state : "none", voice_state ? voice_state : "none");

    free(n); free(t);
    return strdup(buf);
}

char *json_group_peer(const char *name, const char *role, const char *status, const char *connection_status)
{
    char *n = strdup_safe(name);
    char buf[512];

    snprintf(buf, sizeof(buf),
        "{\"name\":\"%s\",\"role\":\"%s\",\"status\":\"%s\",\"connection_status\":\"%s\"}",
        n, role ? role : "user", status ? status : "none", connection_status ? connection_status : "none");

    free(n);
    return strdup(buf);
}

char *json_message_sent(uint32_t message_id)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"message_id\":%u}", message_id);
    return strdup(buf);
}

char *json_success(const char *message)
{
    char *msg = strdup_safe(message);
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"success\":true,\"message\":\"%s\"}", msg);
    free(msg);
    return strdup(buf);
}

char *json_event(const char *event_type, const char *data)
{
    char buf[2048];
    snprintf(buf, sizeof(buf), "{\"event_type\":\"%s\",\"data\":%s}", event_type, data);
    return strdup(buf);
}

static uint64_t event_id_counter = 0;

char *json_event_with_id(const char *event_type, const char *data)
{
    uint64_t id = __atomic_fetch_add(&event_id_counter, 1, __ATOMIC_RELAXED);
    char buf[2048];
    snprintf(buf, sizeof(buf), "{\"event_id\":%lu,\"event_type\":\"%s\",\"data\":%s}",
             (unsigned long)id, event_type, data);
    return strdup(buf);
}