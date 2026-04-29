#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>
#include <pthread.h>

#define MAX_EVENTS 256
#define MAX_EVENT_DATA_SIZE 1024

typedef struct Event {
    uint64_t        event_id;
    char            event_type[32];
    uint64_t        timestamp;
    char            data[MAX_EVENT_DATA_SIZE];
    uint8_t         delivered_sse;
    uint8_t         delivered_ws;
    uint8_t         delivered_poll;
} Event;

typedef struct EventQueue {
    Event       events[MAX_EVENTS];
    uint64_t    write_pos;
    uint64_t    read_pos;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} EventQueue;

int event_queue_init(EventQueue *q);
void event_queue_destroy(EventQueue *q);
int event_queue_push(EventQueue *q, const char *event_type, const char *data);
int event_queue_pop(EventQueue *q, Event *event);
int event_queue_get(EventQueue *q, uint64_t after_id, Event *events, int max_events);

#endif /* EVENT_QUEUE_H */