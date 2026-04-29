#include "push_sse.h"
#include "json_util.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_SSE_CLIENTS 64

typedef struct SSEClient {
    struct mg_connection *nc;
    uint64_t             last_event_id;
} SSEClient;

struct SSEDriver {
    SSEClient clients[MAX_SSE_CLIENTS];
    int       client_count;
    pthread_mutex_t lock;
};

SSEDriver *sse_driver_init(void)
{
    SSEDriver *driver = calloc(1, sizeof(SSEDriver));
    pthread_mutex_init(&driver->lock, NULL);
    return driver;
}

void sse_driver_destroy(SSEDriver *driver)
{
    if (driver) {
        pthread_mutex_destroy(&driver->lock);
        free(driver);
    }
}

void sse_driver_add_client(SSEDriver *driver, struct mg_connection *nc)
{
    pthread_mutex_lock(&driver->lock);
    if (driver->client_count < MAX_SSE_CLIENTS) {
        driver->clients[driver->client_count].nc = nc;
        driver->clients[driver->client_count].last_event_id = 0;
        driver->client_count++;
    }
    pthread_mutex_unlock(&driver->lock);
}

void sse_driver_remove_client(SSEDriver *driver, struct mg_connection *nc)
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

void sse_driver_broadcast(SSEDriver *driver, const Event *event)
{
    pthread_mutex_lock(&driver->lock);

    for (int i = 0; i < driver->client_count; i++) {
        SSEClient *client = &driver->clients[i];
        if (client->nc && !client->nc->is_closing) {
            char *json = json_event_with_id(event->event_type, event->data);
            mg_printf(client->nc, "data: %s\n\n", json);
            free(json);
        }
    }

    pthread_mutex_unlock(&driver->lock);
}