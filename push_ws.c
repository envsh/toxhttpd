#include "push_ws.h"
#include "json_util.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_WS_CLIENTS 64

typedef struct WSClient {
    struct mg_connection *nc;
    uint64_t             last_event_id;
} WSClient;

struct WSPushDriver {
    WSClient  clients[MAX_WS_CLIENTS];
    int       client_count;
    pthread_mutex_t lock;
};

WSPushDriver *ws_driver_init(void)
{
    WSPushDriver *driver = calloc(1, sizeof(WSPushDriver));
    pthread_mutex_init(&driver->lock, NULL);
    return driver;
}

void ws_driver_destroy(WSPushDriver *driver)
{
    if (driver) {
        pthread_mutex_destroy(&driver->lock);
        free(driver);
    }
}

void ws_driver_add_client(WSPushDriver *driver, struct mg_connection *nc)
{
    pthread_mutex_lock(&driver->lock);
    if (driver->client_count < MAX_WS_CLIENTS) {
        driver->clients[driver->client_count].nc = nc;
        driver->clients[driver->client_count].last_event_id = 0;
        driver->client_count++;
    }
    pthread_mutex_unlock(&driver->lock);
}

void ws_driver_remove_client(WSPushDriver *driver, struct mg_connection *nc)
{
    pthread_mutex_lock(&driver->lock);
    for (int i = 0; i < driver->client_count; i++) {
        if (driver->clients[i].nc == nc) {
            driver->clients[i] = driver->clients[driver->client_count - 1];
            driver->client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&driver->lock);
}

void ws_driver_broadcast(WSPushDriver *driver, const Event *event)
{
    pthread_mutex_lock(&driver->lock);

    char ws_frame[4096];
    snprintf(ws_frame, sizeof(ws_frame),
        "{\"type\":\"event\",\"event_type\":\"%s\",\"data\":%s}",
        event->event_type, event->data);

    for (int i = 0; i < driver->client_count; i++) {
        WSClient *client = &driver->clients[i];
        if (client->nc && !client->nc->is_closing) {
            mg_ws_send(client->nc, ws_frame, strlen(ws_frame), WEBSOCKET_OP_TEXT);
        }
    }

    pthread_mutex_unlock(&driver->lock);
}