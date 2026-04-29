#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stddef.h>
#include <stdint.h>

char *json_error(int code, const char *message);
char *json_self(const char *address, const char *name, const char *status, const char *status_emoji, const char *connection_status);
char *json_friend_list(const uint32_t *friend_ids, size_t count);
char *json_friend_info(uint32_t friend_id, const char *name, const char *status, int status_enum, const uint8_t *pubkey, const char *connection_status, const char *self_connection_status, const char *self_address);
char *json_group_list(const uint32_t *group_ids, size_t count);
char *json_group_info(uint32_t group_id, const char *name, const char *topic, const char *privacy_state, const char *voice_state);
char *json_group_peer(const char *name, const char *role, const char *status, const char *connection_status);
char *json_message_sent(uint32_t message_id);
char *json_success(const char *message);
char *json_event(const char *event_type, const char *data);
char *json_event_with_id(const char *event_type, const char *data);

#endif /* JSON_UTIL_H */