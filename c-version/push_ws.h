#ifndef PUSH_WS_H
#define PUSH_WS_H

#include "mongoose.h"
#include "event_queue.h"

typedef struct WSPushDriver WSPushDriver;

WSPushDriver *ws_driver_init(void);
void ws_driver_destroy(WSPushDriver *driver);
void ws_driver_add_client(WSPushDriver *driver, struct mg_connection *nc);
void ws_driver_remove_client(WSPushDriver *driver, struct mg_connection *nc);
void ws_driver_broadcast(WSPushDriver *driver, const Event *event);

#endif /* PUSH_WS_H */