#ifndef PUSH_SSE_H
#define PUSH_SSE_H

#include "mongoose.h"
#include "event_queue.h"

typedef struct SSEDriver SSEDriver;

SSEDriver *sse_driver_init(void);
void sse_driver_destroy(SSEDriver *driver);
void sse_driver_add_client(SSEDriver *driver, struct mg_connection *nc);
void sse_driver_remove_client(SSEDriver *driver, struct mg_connection *nc);
void sse_driver_broadcast(SSEDriver *driver, const Event *event);

#endif /* PUSH_SSE_H */