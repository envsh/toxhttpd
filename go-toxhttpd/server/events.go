package server

import (
	"sync"
	"time"
)

type Event struct {
	ID        uint64    `json:"event_id"`
	Type      string    `json:"event_type"`
	Data      string    `json:"data"`
	Timestamp time.Time `json:"timestamp"`
}

const maxQueueSize = 500
const eventTimeWindow = 30 * time.Second
const staleStatusAge = 60 * time.Second

var dropPriority = map[string]int{
	"friend_message":           1,
	"conference_message":       1,
	"group_message":            1,
	"friend_status":            2,
	"friend_status_message":    2,
	"friend_connection_status": 2,
	"friend_typing":            2,
	"friend_read_receipt":      2,
}

func isProtectedEvent(eventType string) bool {
	return eventType == "group_invite" || eventType == "conference_invite"
}

type EventQueue struct {
	mu     sync.Mutex
	events []Event
	nextID uint64
}

func NewEventQueue() *EventQueue {
	return &EventQueue{
		events: make([]Event, 0),
		nextID: 1,
	}
}

func (q *EventQueue) dropOne() {
	now := time.Now()
	bestIdx := -1
	bestPriority := 0

	for i, e := range q.events {
		if isProtectedEvent(e.Type) {
			continue
		}
		prio, ok := dropPriority[e.Type]
		if !ok {
			prio = 9
		}
		if prio == 2 && now.Sub(e.Timestamp) < staleStatusAge {
			continue
		}
		if bestIdx == -1 || prio > bestPriority {
			bestIdx = i
			bestPriority = prio
		}
	}

	if bestIdx == -1 {
		bestIdx = 0
	}
	q.events = append(q.events[:bestIdx], q.events[bestIdx+1:]...)
}

func (q *EventQueue) Push(eventType string, data string) uint64 {
	q.mu.Lock()
	defer q.mu.Unlock()

	event := Event{
		ID:        q.nextID,
		Type:      eventType,
		Data:      data,
		Timestamp: time.Now(),
	}
	q.events = append(q.events, event)
	q.nextID++

	for len(q.events) > maxQueueSize {
		q.dropOne()
	}
	return event.ID
}

func (q *EventQueue) DeleteEvent(id uint64) {
	q.mu.Lock()
	defer q.mu.Unlock()

	newEvents := make([]Event, 0, len(q.events))
	for _, e := range q.events {
		if e.ID != id {
			newEvents = append(newEvents, e)
		}
	}
	q.events = newEvents
}

func (q *EventQueue) PopAfter(after uint64) []Event {
	q.mu.Lock()
	defer q.mu.Unlock()

	cutoff := time.Now().Add(-eventTimeWindow)
	result := make([]Event, 0)
	for _, e := range q.events {
		if e.ID > after {
			if after == 0 && e.Timestamp.Before(cutoff) {
				continue
			}
			result = append(result, e)
		}
	}
	return result
}

func (q *EventQueue) GetNextID() uint64 {
	q.mu.Lock()
	defer q.mu.Unlock()
	return q.nextID
}
