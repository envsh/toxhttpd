package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"time"

	"github.com/anomalyco/toxhttpd-go/tox"
)

// Event represents an event to be sent to clients
type Event struct {
	ID        uint64    `json:"event_id"`
	Type      string    `json:"event_type"`
	Data      string    `json:"data"`
	Timestamp time.Time `json:"timestamp"`
}

// EventQueue is a thread-safe event queue
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
	return event.ID
}

func (q *EventQueue) PopAfter(after uint64) []Event {
	q.mu.Lock()
	defer q.mu.Unlock()

	result := make([]Event, 0)
	for _, e := range q.events {
		if e.ID > after {
			result = append(result, e)
		}
	}
	return result
}

// Server holds the server state
type Server struct {
	tox        *tox.Tox
	eventQueue *EventQueue
}

func NewServer() (*Server, error) {
	t, err := tox.NewTox()
	if err != nil {
		return nil, err
	}

	server := &Server{
		tox:        t,
		eventQueue: NewEventQueue(),
	}

	log.Println("Server initialized with tox callbacks")
	return server, nil
}

func (s *Server) handleSelf(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	addr := s.tox.GetAddress()
	name := s.tox.GetSelfName()
	status := s.tox.GetSelfStatus()
	connStatus := s.tox.GetConnectionStatus()

	resp := map[string]interface{}{
		"address":           addr,
		"name":              name,
		"status_message":    status,
		"status_emoji":      "",
		"connection_status": connStatus,
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleFriends(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		friends := s.tox.GetFriendList()
		resp := map[string]interface{}{
			"friends": friends,
		}
		json.NewEncoder(w).Encode(resp)

	} else if r.Method == http.MethodPost {
		r.ParseForm()
		pubkey := r.FormValue("public_key")
		if pubkey == "" {
			http.Error(w, `{"error":"missing public_key"}`, http.StatusBadRequest)
			return
		}

		fn, err := s.tox.AddFriend(pubkey)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		resp := map[string]interface{}{
			"friend_id": fn,
			"message":   "friend added",
		}
		json.NewEncoder(w).Encode(resp)
	}
}

func (s *Server) handleFriendInfo(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	r.ParseForm()
	friendIDStr := r.FormValue("friend_id")
	var friendID uint32
	fmt.Sscanf(friendIDStr, "%d", &friendID)

	name := s.tox.GetFriendName(friendID)
	connStatus := s.tox.GetFriendConnectionStatus(friendID)
	pk := s.tox.GetFriendPublicKey(friendID)

	resp := map[string]interface{}{
		"friend_id":               friendID,
		"name":                    name,
		"status":                  "",
		"status_enum":             0,
		"public_key":              pk,
		"connection_status":       connStatus,
		"self_connection_status": "offline",
		"self_address":            "",
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleSendMessage(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	r.ParseForm()
	friendIDStr := r.FormValue("friend_id")
	message := r.FormValue("message")

	var friendID uint32
	fmt.Sscanf(friendIDStr, "%d", &friendID)

	msgID, err := s.tox.SendMessage(friendID, message)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	resp := map[string]interface{}{
		"message_id": msgID,
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleGroups(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		groups := s.tox.GetConferenceList()
		resp := map[string]interface{}{
			"groups": groups,
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
		groupID, err := s.tox.NewConference()
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}
		resp := map[string]interface{}{
			"group_id": groupID,
		}
		json.NewEncoder(w).Encode(resp)
	}
}

func (s *Server) handleBootstrap(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	log.Println("Bootstrap requested")
	resp := map[string]interface{}{
		"message": "bootstrap initiated",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleEvents(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	after := r.URL.Query().Get("after")
	var afterID uint64
	fmt.Sscanf(after, "%d", &afterID)

	// Long polling: wait for new events or timeout
	timeout := time.After(30 * time.Second)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-timeout:
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		case <-ticker.C:
			events := s.eventQueue.PopAfter(afterID)
			if len(events) > 0 {
				w.Header().Set("Content-Type", "application/json")
				json.NewEncoder(w).Encode(events)
				return
			}
		}
	}
}

func (s *Server) handleWeb(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Path
	if path == "/" {
		path = "/web/index.html"
	}

	absPath := "/home/gzleo/aprog/toxhttpd" + path
	http.ServeFile(w, r, absPath)
}

func (s *Server) Start(port string) error {
	http.HandleFunc("/api/self", s.handleSelf)
	http.HandleFunc("/api/friends", s.handleFriends)
	http.HandleFunc("/api/friend", s.handleFriendInfo)
	http.HandleFunc("/api/messages", s.handleSendMessage)
	http.HandleFunc("/api/groups", s.handleGroups)
	http.HandleFunc("/api/bootstrap", s.handleBootstrap)
	http.HandleFunc("/api/events", s.handleEvents)
	http.HandleFunc("/", s.handleWeb)

	log.Printf("Server starting on :%s", port)
	return http.ListenAndServe(":"+port, nil)
}

func main() {
	log.SetOutput(os.Stdout)

	server, err := NewServer()
	if err != nil {
		log.Fatalf("Failed to create server: %v", err)
	}

	// Start tox iteration in background
	go func() {
		for {
			server.tox.Iterate()
			time.Sleep(time.Millisecond * time.Duration(server.tox.IterationInterval()))
		}
	}()

	log.Fatal(server.Start("8181"))
}
