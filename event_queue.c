#include "event_queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int event_queue_init(EventQueue *q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return 0;
}

void event_queue_destroy(EventQueue *q)
{
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}

static uint64_t global_event_id = 0;

int event_queue_push(EventQueue *q, const char *event_type, const char *data)
{
    pthread_mutex_lock(&q->lock);

    uint64_t write_pos = q->write_pos;
    uint64_t next_pos = (write_pos + 1) % MAX_EVENTS;

    while (next_pos == q->read_pos) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    Event *e = &q->events[write_pos % MAX_EVENTS];
    e->event_id = __atomic_fetch_add(&global_event_id, 1, __ATOMIC_RELAXED);
    strncpy(e->event_type, event_type, sizeof(e->event_type) - 1);
    e->timestamp = time(NULL);
    strncpy(e->data, data, sizeof(e->data) - 1);
    e->delivered_sse = 0;
    e->delivered_ws = 0;
    e->delivered_poll = 0;

    q->write_pos = write_pos + 1;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int event_queue_pop(EventQueue *q, Event *event)
{
    pthread_mutex_lock(&q->lock);

    while (q->read_pos == q->write_pos) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    Event *e = &q->events[q->read_pos % MAX_EVENTS];
    *event = *e;
    q->read_pos++;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int event_queue_get(EventQueue *q, uint64_t after_id, Event *events, int max_events)
{
    pthread_mutex_lock(&q->lock);

    int count = 0;
    uint64_t pos = q->read_pos;

    while (pos < q->write_pos && count < max_events) {
        Event *e = &q->events[pos % MAX_EVENTS];
        if (e->event_id > after_id) {
            events[count++] = *e;
        }
        pos++;
    }

    pthread_mutex_unlock(&q->lock);
    return count;
}