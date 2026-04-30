package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"

	tox "github.com/TokTok/go-toxcore-c"
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
	tox                  *tox.Tox
	eventQueue           *EventQueue
	selfConnectionStatus string
	friendStatuses       map[uint32]string
	mu                   sync.RWMutex
}

func NewServer() (*Server, error) {
	t := tox.NewTox(nil)
	if t == nil {
		return nil, fmt.Errorf("failed to create tox instance")
	}

	server := &Server{
		tox:                  t,
		eventQueue:           NewEventQueue(),
		selfConnectionStatus: "offline",
		friendStatuses:       make(map[uint32]string),
	}

	setupCallbacks(server)

	log.Println("Server initialized with tox callbacks")
	return server, nil
}

func setupCallbacks(s *Server) {
	// Self connection status callback
	s.tox.CallbackSelfConnectionStatus(func(this *tox.Tox, status int, userData interface{}) {
		s.mu.Lock()
		defer s.mu.Unlock()
		switch status {
		case tox.CONNECTION_NONE:
			s.selfConnectionStatus = "offline"
		case tox.CONNECTION_TCP:
			s.selfConnectionStatus = "tcp"
		case tox.CONNECTION_UDP:
			s.selfConnectionStatus = "udp"
		default:
			s.selfConnectionStatus = "unknown"
		}
		s.eventQueue.Push("self_connection_status", s.selfConnectionStatus)
	}, nil)

	// Friend request callback
	s.tox.CallbackFriendRequest(func(this *tox.Tox, pubkey string, message string, userData interface{}) {
		// pubkey is hex string
		_, err := s.tox.FriendAddNorequest(pubkey)
		if err != nil {
			log.Printf("Failed to accept friend request: %v", err)
		} else {
			s.eventQueue.Push("friend_request", pubkey)
		}
	}, nil)

	// Friend message callback
	s.tox.CallbackFriendMessage(func(this *tox.Tox, friendNumber uint32, message string, userData interface{}) {
		s.eventQueue.Push("friend_message", fmt.Sprintf("%d:%s", friendNumber, message))
	}, nil)

	// Friend connection status callback
	s.tox.CallbackFriendConnectionStatus(func(this *tox.Tox, friendNumber uint32, connectionStatus int, userData interface{}) {
		s.mu.Lock()
		defer s.mu.Unlock()
		var status string
		switch connectionStatus {
		case tox.CONNECTION_NONE:
			status = "offline"
		case tox.CONNECTION_TCP:
			status = "tcp"
		case tox.CONNECTION_UDP:
			status = "udp"
		default:
			status = "unknown"
		}
		s.friendStatuses[friendNumber] = status
		s.eventQueue.Push("friend_connection_status", fmt.Sprintf("%d:%s", friendNumber, status))
	}, nil)

	// Friend name callback
	s.tox.CallbackFriendName(func(this *tox.Tox, friendNumber uint32, newName string, userData interface{}) {
		s.eventQueue.Push("friend_name", fmt.Sprintf("%d:%s", friendNumber, newName))
	}, nil)
}

func (s *Server) handleSelf(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	addr := s.tox.SelfGetAddress()
	name := s.tox.SelfGetName()
	status, _ := s.tox.SelfGetStatusMessage()

	s.mu.RLock()
	connStatus := s.selfConnectionStatus
	s.mu.RUnlock()

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
		friends := s.tox.SelfGetFriendList()
		resp := map[string]interface{}{
			"friends": friends,
		}
		json.NewEncoder(w).Encode(resp)

	} else if r.Method == http.MethodPost {
		r.ParseForm()
		pubkeyStr := r.FormValue("public_key")
		message := r.FormValue("message")
		if pubkeyStr == "" {
			http.Error(w, `{"error":"missing public_key"}`, http.StatusBadRequest)
			return
		}
		if message == "" {
			message = "Hello, I'm using toxhttpd-go!"
		}

		// pubkeyStr should be 64-char hex (public key) or 76-char hex (address)
		fn, err := s.tox.FriendAdd(pubkeyStr, message)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		resp := map[string]interface{}{
			"friend_id": fn,
			"message":   "friend request sent",
		}
		json.NewEncoder(w).Encode(resp)

	} else if r.Method == http.MethodDelete {
		r.ParseForm()
		friendIDStr := r.FormValue("friend_id")
		var friendID uint32
		fmt.Sscanf(friendIDStr, "%d", &friendID)

		_, err := s.tox.FriendDelete(friendID)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		resp := map[string]interface{}{
			"message": "friend deleted",
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

	name, _ := s.tox.FriendGetName(friendID)
	pk, _ := s.tox.FriendGetPublicKey(friendID)

	s.mu.RLock()
	connStatus := s.friendStatuses[friendID]
	s.mu.RUnlock()

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

	msgID, err := s.tox.FriendSendMessage(friendID, message)
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
		// Return group list (different from conferences)
		// For now return empty
		resp := map[string]interface{}{
			"groups": []uint32{},
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
		// Using conference as group
		// Note: go-toxcore-c uses ConferenceNew()
		resp := map[string]interface{}{
			"group_id": 0,
		}
		json.NewEncoder(w).Encode(resp)
	}
}

func (s *Server) handleConferences(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		// go-toxcore-c may need to track conference list ourselves
		resp := map[string]interface{}{
			"conferences": []uint32{},
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
		resp := map[string]interface{}{
			"conference_id": 0,
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
	// Use bootstrap nodes from bootstrap.json
	s.tox.Bootstrap("144.217.167.73", 33445, "7E5668E0EE09E19F320AD47902419331FFEE147BB3606769CFBE921A2A2FD34C")
	s.tox.Bootstrap("tox.abilinski.com", 33445, "10C00EB250C3233E343E2AEBA07115A5C28920E9C8D29492F6D00B29049EDC7E")
	s.tox.Bootstrap("tox1.mf-net.eu", 33445, "B3E5FA80DC8EBD1149AD2AB35ED8B85BD546DEDE261CA593234C619249419506")

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

func loggingMiddleware(next http.HandlerFunc) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		log.Printf("[HTTP] %s %s from %s", r.Method, r.URL.Path, r.RemoteAddr)
		next(w, r)
	}
}

func (s *Server) Start(port string) error {
	http.HandleFunc("/api/self", loggingMiddleware(s.handleSelf))
	http.HandleFunc("/api/friends", loggingMiddleware(s.handleFriends))
	http.HandleFunc("/api/friend_delete", loggingMiddleware(s.handleFriendDelete))
	http.HandleFunc("/api/friend", loggingMiddleware(s.handleFriendInfo))
	http.HandleFunc("/api/messages", loggingMiddleware(s.handleSendMessage))
	http.HandleFunc("/api/groups", loggingMiddleware(s.handleGroups))
	http.HandleFunc("/api/conferences", loggingMiddleware(s.handleConferences))
	http.HandleFunc("/api/bootstrap", loggingMiddleware(s.handleBootstrap))
	http.HandleFunc("/api/events", loggingMiddleware(s.handleEvents))
	http.HandleFunc("/", loggingMiddleware(s.handleWeb))

	log.Printf("Server starting on :%s", port)
	return http.ListenAndServe(":"+port, nil)
}

func main() {
	log.SetFlags(log.Lshortfile | log.LstdFlags)

	server, err := NewServer()
	if err != nil {
		log.Fatalf("Failed to create server: %v", err)
	}

	// Add test data if no friends/groups exist
	addTestData(server)

	// Bootstrap to Tox network
	server.tox.Bootstrap("144.217.167.73", 33445, "7E5668E0EE09E19F320AD47902419331FFEE147BB3606769CFBE921A2A2FD34C")

	// Start tox iteration in background
	go func() {
		for {
			server.tox.Iterate()
			time.Sleep(time.Millisecond * time.Duration(server.tox.IterationInterval()))
		}
	}()

	// Handle shutdown signals
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sig
		log.Println("Shutting down...")
		// Save data
		os.MkdirAll("data", 0700)
		server.tox.WriteSavedata("data/savedata.bin")
		server.tox.Kill()
		os.Exit(0)
	}()

	log.Fatal(server.Start("8181"))
}

// addTestData adds some test friends and conferences if none exist
func addTestData(s *Server) {
	friends := s.tox.SelfGetFriendList()
	if len(friends) == 0 {
		log.Println("No friends found, adding test friend...")
		log.Println("Note: Add friends via /api/friends POST with public_key")
	}

	addr := s.tox.SelfGetAddress()
	log.Printf("Tox ID: %s", addr)
	log.Println("Server ready with saved tox identity")
}

func (s *Server) handleFriendDelete(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete && r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	r.ParseForm()
	friendIDStr := r.FormValue("friend_id")
	var friendID uint32
	fmt.Sscanf(friendIDStr, "%d", &friendID)

	_, err := s.tox.FriendDelete(friendID)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	resp := map[string]interface{}{
		"message": "friend deleted",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}
