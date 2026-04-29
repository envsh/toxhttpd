#ifndef TOX_CORE_H
#define TOX_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <tox/tox.h>
#include "event_queue.h"

typedef struct ToxCore ToxCore;

ToxCore *tox_core_init(EventQueue *event_queue);
void tox_core_destroy(ToxCore *core);
Tox *tox_core_get_tox(ToxCore *core);

bool tox_core_bootstrap(ToxCore *core, const char *address, uint16_t port, const uint8_t *pubkey);

char *tox_core_get_self_address(ToxCore *core);
char *tox_core_get_self_name(ToxCore *core);
char *tox_core_get_self_status(ToxCore *core);
bool tox_core_set_self_name(ToxCore *core, const char *name);
bool tox_core_set_self_status(ToxCore *core, const char *status);

int tox_core_get_friend_list(ToxCore *core, uint32_t *friends, size_t max_count);
char *tox_core_get_friend_name(ToxCore *core, uint32_t friend_id);
char *tox_core_get_friend_status(ToxCore *core, uint32_t friend_id);
bool tox_core_friend_add_norequest(ToxCore *core, const uint8_t *pubkey);
bool tox_core_friend_delete(ToxCore *core, uint32_t friend_id);

uint32_t tox_core_friend_send_message(ToxCore *core, uint32_t friend_id, const char *message);

int tox_core_get_group_list(ToxCore *core, uint32_t *groups, size_t max_count);
uint32_t tox_core_group_new(ToxCore *core);
bool tox_core_group_join(ToxCore *core, const uint8_t *chat_id, size_t length);
bool tox_core_group_leave(ToxCore *core, uint32_t group_id);
char *tox_core_group_get_name(ToxCore *core, uint32_t group_id);
char *tox_core_group_get_topic(ToxCore *core, uint32_t group_id);
bool tox_core_group_set_topic(ToxCore *core, uint32_t group_id, const char *topic);
bool tox_core_group_set_self_name(ToxCore *core, uint32_t group_id, const char *name);
int tox_core_group_get_peers(ToxCore *core, uint32_t group_id, uint32_t *peer_ids, size_t max_count);
char *tox_core_group_get_peer_name(ToxCore *core, uint32_t group_id, uint32_t peer_id);
uint32_t tox_core_group_send_message(ToxCore *core, uint32_t group_id, const char *message);

#endif /* TOX_CORE_H */