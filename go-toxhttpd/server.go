package main

import (
	"database/sql"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net/http"
	"net/url"
	"os"
	"os/signal"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"

	_ "github.com/mattn/go-sqlite3"
	tox "github.com/TokTok/go-toxcore-c"
)

// BootstrapNode represents a Tox bootstrap node
type BootstrapNode struct {
	IPv4       string
	IPv6       string
	Port       uint16
	PublicKey  string
	Maintainer string
}

// Bootstrap nodes from C version (bootstrap.c)
var bootstrapNodes = []BootstrapNode{
	{
		IPv4:       "104.225.141.59",
		IPv6:       "-",
		Port:       33445,
		PublicKey:  "933BA20B2E258B4C0D475B6DECE90C7E827FE83EFA9655414E7841251B19A72C",
		Maintainer: "velusip (C version)",
	},
	{
		IPv4:       "43.198.227.166",
		IPv6:       "-",
		Port:       3389,
		PublicKey:  "AD13AB0D434BCE6C83FE2649237183964AE3341D0AFB3BE1694B18505E4E135E",
		Maintainer: "AnthonyBilinski (C version)",
	},
	{
		IPv4:       "3.0.24.15",
		IPv6:       "-",
		Port:       33445,
		PublicKey:  "E20ABCF38CDBFFD7D04B29C956B33F7B27A3BB7AF0618101617B036E4AEA402D",
		Maintainer: "2mf (C version)",
	},
	{
      IPv4: "144.217.167.73",
      IPv6: "-",
      Port: 33445,
      PublicKey: "7E5668E0EE09E19F320AD47902419331FFEE147BB3606769CFBE921A2A2FD34C",
      Maintainer: "velusip",
    },
    {
      IPv4: "tox.abilinski.com",
      IPv6: "-",
      Port: 33445,
      PublicKey: "10C00EB250C3233E343E2AEBA07115A5C28920E9C8D29492F6D00B29049EDC7E",
      Maintainer: "AnthonyBilinski",
    },
    {
      IPv4: "86.107.187.54",
      IPv6: "-",
      Port: 33445,
      PublicKey: "2C0F90965134C7BEFAFE72B077A19221628D7045BB51C1165A2C75CDB2B32634",
      Maintainer: "Boca",
    },	
	
}

// bootstrapAll bootstraps the Tox instance to all nodes (UDP + TCP relay)
func bootstrapAll(t *tox.Tox) {
	for i, node := range bootstrapNodes {
		log.Printf("Bootstrap %d: UDP %s:%d (maintainer: %s)", i, node.IPv4, node.Port, node.Maintainer)
		t.Bootstrap(node.IPv4, node.Port, node.PublicKey)
		log.Printf("Bootstrap %d: TCP relay %s:%d (maintainer: %s)", i, node.IPv4, node.Port, node.Maintainer)
		// Note: go-toxcore-c's Bootstrap function handles both UDP and TCP
	}
}

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

// DeleteEvent removes an event from the queue by ID
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

	result := make([]Event, 0)
	for _, e := range q.events {
		if e.ID > after {
			result = append(result, e)
		}
	}
	return result
}

// RequestParams provides a unified way to access request parameters
// regardless of content type (JSON or form-urlencoded)
type RequestParams struct {
	formValues url.Values
	jsonData   map[string]interface{}
	isJSON     bool
}

// getRequestParams parses request parameters based on Content-Type header
func getRequestParams(r *http.Request) (*RequestParams, error) {
	params := &RequestParams{}
	contentType := r.Header.Get("Content-Type")

	if strings.Contains(contentType, "application/json") {
		params.isJSON = true
		params.jsonData = make(map[string]interface{})
		if err := json.NewDecoder(r.Body).Decode(&params.jsonData); err != nil {
			return nil, fmt.Errorf("failed to parse JSON: %v", err)
		}
	} else {
		if err := r.ParseForm(); err != nil {
			return nil, fmt.Errorf("failed to parse form: %v", err)
		}
		params.formValues = r.Form
	}
	return params, nil
}

// Get returns the value for the given key
func (p *RequestParams) Get(key string) string {
	if p.isJSON {
		val, ok := p.jsonData[key]
		if !ok {
			return ""
		}
		switch v := val.(type) {
		case string:
			return v
		case float64:
			return strconv.FormatFloat(v, 'f', -1, 64)
		default:
			return fmt.Sprintf("%v", v)
		}
	}
	return p.formValues.Get(key)
}

// ContactNotFound is returned by getContactNumber when contact is not found
// It is math.MaxUint32 (0xFFFFFFFF), which when cast to int32 is -1
const ContactNotFound = uint32(0xFFFFFFFF)

// Server holds the server state
type Server struct {
	tox                  *tox.Tox
	db                   *sql.DB  // SQLite connection for event copy only
	eventQueue           *EventQueue
	selfConnectionStatus string
	friendStatuses       map[uint32]string
	conferenceConnected  map[uint32]bool  // 新增：会议连接状态
	mu                   sync.RWMutex
}

func NewServer(udpEnabled bool) (*Server, error) {
	// Load saved tox data if exists
	saveDataPath := "data/savedata.bin"
	var saveData []byte
	if data, err := os.ReadFile(saveDataPath); err == nil && len(data) > 100 {
		saveData = data
		log.Printf("[TOX] Loaded save data from %s (%d bytes)", saveDataPath, len(data))
	} else {
		log.Printf("[TOX] No save data found, creating new identity")
	}

	// Create Tox instance with options
	opts := tox.NewToxOptions()
	opts.Udp_enabled = udpEnabled
	opts.GroupsPersistence = true
	if saveData != nil {
		opts.Savedata_type = tox.SAVEDATA_TYPE_TOX_SAVE
		opts.Savedata_data = saveData
	}
	t := tox.NewTox(opts)
	if t == nil {
		return nil, fmt.Errorf("failed to create tox instance")
	}

	// Initialize SQLite database for message history
	db, err := initMsgHistDB("data/msghist.db")
	if err != nil {
		return nil, fmt.Errorf("failed to init msghist db: %w", err)
	}

	server := &Server{
		tox:                  t,
		db:                   db,
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
		var statusStr string
		switch status {
		case tox.CONNECTION_NONE:
			s.selfConnectionStatus = "offline"
			statusStr = "OFFLINE"
		case tox.CONNECTION_TCP:
			s.selfConnectionStatus = "tcp"
			statusStr = "TCP"
		case tox.CONNECTION_UDP:
			s.selfConnectionStatus = "udp"
			statusStr = "UDP"
		default:
			s.selfConnectionStatus = "unknown"
			statusStr = "UNKNOWN"
		}
		log.Printf("[TOX_CALLBACK] SelfConnectionStatus: %s (%d)", statusStr, status)
		data, _ := json.Marshal(map[string]interface{}{"status": s.selfConnectionStatus})
		s.eventQueue.Push("self_connection_status", string(data))
	}, nil)

	// Friend request callback
	s.tox.CallbackFriendRequest(func(this *tox.Tox, pubkey string, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendRequest: pubkey=%s, message=%s", pubkey, message)
		// pubkey is hex string
		_, err := s.tox.FriendAddNorequest(pubkey)
		if err != nil {
			log.Printf("Failed to accept friend request: %v", err)
		} else {
			data, _ := json.Marshal(map[string]interface{}{"public_key": pubkey})
			s.eventQueue.Push("friend_request", string(data))
		}
	}, nil)

	// Friend message callback
	s.tox.CallbackFriendMessage(func(this *tox.Tox, friendNumber uint32, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendMessage: friend=%d, message=%s", friendNumber, message)
		friendPubKey, _ := s.tox.FriendGetPublicKey(friendNumber)
		chanidInt, _ := s.getOrCreatePubKeyID(friendPubKey)
		senderInt, _ := s.getOrCreatePubKeyID(friendPubKey)
		data, _ := json.Marshal(map[string]interface{}{
			"message":   message,
			"sender":    senderInt,
			"direction": "received",
			"friend_id": friendNumber,
		})
		s.eventQueue.Push("friend_message", string(data))
		// Persist to SQLite with integer chanid
		s.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	// Friend connection status callback
	s.tox.CallbackFriendConnectionStatus(func(this *tox.Tox, friendNumber uint32, connectionStatus int, userData interface{}) {
		s.mu.Lock()
		defer s.mu.Unlock()
		var status string
		switch connectionStatus {
		case tox.CONNECTION_NONE:
			status = "offline"
			log.Printf("[TOX_CALLBACK] FriendConnectionStatus: friend=%d, OFFLINE (%d)", friendNumber, connectionStatus)
		case tox.CONNECTION_TCP:
			status = "tcp"
			log.Printf("[TOX_CALLBACK] FriendConnectionStatus: friend=%d, TCP (%d)", friendNumber, connectionStatus)
		case tox.CONNECTION_UDP:
			status = "udp"
			log.Printf("[TOX_CALLBACK] FriendConnectionStatus: friend=%d, UDP (%d)", friendNumber, connectionStatus)
		default:
			status = "unknown"
			log.Printf("[TOX_CALLBACK] FriendConnectionStatus: friend=%d, UNKNOWN (%d)", friendNumber, connectionStatus)
		}
		s.friendStatuses[friendNumber] = status
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"status":    status,
		})
		s.eventQueue.Push("friend_connection_status", string(data))
	}, nil)

	// Friend name callback
	s.tox.CallbackFriendName(func(this *tox.Tox, friendNumber uint32, newName string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendName: friend=%d, name=%s", friendNumber, newName)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"name":      newName,
		})
		s.eventQueue.Push("friend_name", string(data))
	}, nil)

	// Friend status message callback (stub)
	s.tox.CallbackFriendStatusMessage(func(this *tox.Tox, friendNumber uint32, newStatus string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendStatusMessage: friend=%d, status=%s", friendNumber, newStatus)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id":      friendNumber,
			"status_message": newStatus,
		})
		s.eventQueue.Push("friend_status_message", string(data))
	}, nil)

	// Friend status callback (stub)
	s.tox.CallbackFriendStatus(func(this *tox.Tox, friendNumber uint32, status int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendStatus: friend=%d, status=%d", friendNumber, status)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"status":    status,
		})
		s.eventQueue.Push("friend_status", string(data))
	}, nil)

	// Friend typing callback (stub)
	s.tox.CallbackFriendTyping(func(this *tox.Tox, friendNumber uint32, isTyping uint8, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendTyping: friend=%d, typing=%v", friendNumber, isTyping)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"typing":    isTyping,
		})
		s.eventQueue.Push("friend_typing", string(data))
	}, nil)

	// Friend read receipt callback (stub)
	s.tox.CallbackFriendReadReceipt(func(this *tox.Tox, friendNumber uint32, receipt uint32, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendReadReceipt: friend=%d, receipt=%d", friendNumber, receipt)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"receipt":   receipt,
		})
		s.eventQueue.Push("friend_read_receipt", string(data))
	}, nil)

	// File recv control callback (stub)
	s.tox.CallbackFileRecvControl(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, control int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecvControl: friend=%d, file=%d, control=%d", friendNumber, fileNumber, control)
	}, nil)

	// File recv callback (stub)
	s.tox.CallbackFileRecv(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, kind uint32, fileSize uint64, fileName string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecv: friend=%d, file=%d, kind=%d, size=%d, name=%s", friendNumber, fileNumber, kind, fileSize, fileName)
	}, nil)

	// File recv chunk callback (stub)
	s.tox.CallbackFileRecvChunk(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, position uint64, data []byte, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecvChunk: friend=%d, file=%d, position=%d, len=%d", friendNumber, fileNumber, position, len(data))
	}, nil)

	// File chunk request callback (stub)
	s.tox.CallbackFileChunkRequest(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, position uint64, length int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileChunkRequest: friend=%d, file=%d, position=%d, length=%d", friendNumber, fileNumber, position, length)
	}, nil)

	// Conference invite callback
	s.tox.CallbackConferenceInvite(func(this *tox.Tox, friendNumber uint32, itype uint8, cookie string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceInvite: friend=%d, type=%d, cookie=%s", friendNumber, itype, cookie)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_number": friendNumber,
			"type":         itype,
			"cookie":       cookie,
		})
		s.eventQueue.Push("conference_invite", string(data))
	}, nil)

	// Conference message callback
	s.tox.CallbackConferenceMessage(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceMessage: group=%d, peer=%d, message=%s", groupNumber, peerNumber, message)
		chatId, _ := s.tox.ConferenceGetIdentifier(groupNumber)
		chanidInt, _ := s.getOrCreatePubKeyID(chatId)
		// Get peer pubkey and convert to integer ID
		peerPubKey, _ := s.tox.ConferencePeerGetPublicKey(groupNumber, peerNumber)
		senderInt, _ := s.getOrCreatePubKeyID(peerPubKey)
		data, _ := json.Marshal(map[string]interface{}{
			"message":            message,
			"sender":             senderInt,
			"direction":          "received",
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
		})
		s.eventQueue.Push("conference_message", string(data))
		// Persist to SQLite with integer chanid
		s.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	// Group message callback (NGC)
	s.tox.CallbackGroupMessage(func(this *tox.Tox, groupNumber tox.GroupNumber, peerNumber tox.GroupPeerNumber, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] GroupMessage: group=%d, peer=%d, message=%s", groupNumber, peerNumber, message)
		chatId, _ := s.tox.GroupGetChatId(groupNumber)
		chanidInt, _ := s.getOrCreatePubKeyID(chatId)
		// Get peer pubkey and convert to integer ID
		peerPubKey, _ := s.tox.GroupPeerGetPublicKey(groupNumber, peerNumber)
		senderInt, _ := s.getOrCreatePubKeyID(peerPubKey)
		data, _ := json.Marshal(map[string]interface{}{
			"message":      message,
			"sender":       senderInt,
			"direction":    "received",
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
		})
		s.eventQueue.Push("group_message", string(data))
		// Persist to SQLite with integer chanid
		s.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	// Conference title callback
	s.tox.CallbackConferenceTitle(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, title string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceTitle: group=%d, peer=%d, title=%s", groupNumber, peerNumber, title)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":      peerNumber,
			"title":            title,
		})
		s.eventQueue.Push("conference_title", string(data))
	}, nil)

	// Conference peer name callback
	s.tox.CallbackConferencePeerName(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, name string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferencePeerName: group=%d, peer=%d, name=%s", groupNumber, peerNumber, name)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":      peerNumber,
			"name":             name,
		})
		s.eventQueue.Push("conference_peer_name", string(data))
	}, nil)

	// Conference peer list changed callback
	s.tox.CallbackConferencePeerListChanged(func(this *tox.Tox, groupNumber uint32, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferencePeerListChanged: group=%d", groupNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
		})
		s.eventQueue.Push("conference_peer_list_changed", string(data))
	}, nil)
	
	// 1. CallbackGroupInvite - 邀请回调
	s.tox.CallbackGroupInvite(func(this *tox.Tox, groupNumber tox.GroupNumber,
		friendNumber uint32, data string, userData interface{}) {
		log.Printf("[GroupInvite] group=%d, friend=%d, data=%s",
			groupNumber, friendNumber, data)
		// 检查 invite data 是否有效
		if data == "" {
			log.Printf("[GroupInvite] WARNING: empty invite data from friend %d, skipping event", friendNumber)
			return
		}
		// 推送群组邀请事件到前端
		eventData, _ := json.Marshal(map[string]interface{}{
			"friend_number": friendNumber,
			"chat_id":      data, // 64字符十六进制chat_id (cookie)
		})
		s.eventQueue.Push("group_invite", string(eventData))
		log.Printf("[GroupInvite] Pushed event: friend=%d, chat_id=%s", friendNumber, data)
	}, nil)
	
	// 2. CallbackGroupSelfJoin - 自己加入群组回调
	s.tox.CallbackGroupSelfJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		userData interface{}) {
		log.Printf("[GroupSelfJoin] group=%d", groupNumber)
		// 推送自己加入群组事件
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": groupNumber,
		})
		s.eventQueue.Push("group_self_join", string(data))
	}, nil)
	
	// 3. CallbackGroupPeerJoin - Peer 加入回调
	s.tox.CallbackGroupPeerJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, userData interface{}) {
		log.Printf("[GroupPeerJoin] group=%d, peer=%d", int(groupNumber), int(peerNumber))
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number": int(peerNumber),
		})
		s.eventQueue.Push("group_peer_join", string(data))
	}, nil)
	
	// 4. CallbackGroupPeerExit - Peer 退出回调
	s.tox.CallbackGroupPeerExit(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, exitType tox.GroupExitType, name string, userData interface{}) {
		log.Printf("[GroupPeerExit] group=%d, peer=%d, type=%s, name=%s",
			int(groupNumber), int(peerNumber), tox.GroupExitTypeToString(exitType), name)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number": int(peerNumber),
			"exit_type":   tox.GroupExitTypeToString(exitType),
			"name":        name,
		})
		s.eventQueue.Push("group_peer_exit", string(data))
	}, nil)
	
	// 5. CallbackGroupPeerStatus - Peer 状态变化回调
	s.tox.CallbackGroupPeerStatus(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, status int, userData interface{}) {
		log.Printf("[GroupPeerStatus] group=%d, peer=%d, status=%d", int(groupNumber), int(peerNumber), status)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number": int(peerNumber),
			"status":      status,
		})
		s.eventQueue.Push("group_peer_status", string(data))
	}, nil)

	// 6. CallbackConferenceConnected - 会议连接状态回调
	s.tox.CallbackConferenceConnected(func(this *tox.Tox, groupNumber uint32, userData interface{}) {
		log.Printf("[ConferenceConnected] group=%d", groupNumber)
		// 更新会议连接状态map
		s.mu.Lock()
		s.conferenceConnected[groupNumber] = true
		s.mu.Unlock()
		// 推送连接状态事件到前端
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"is_connected":     true,
		})
		s.eventQueue.Push("conference_connected", string(data))
	}, nil)

	log.Println("[TOX] All callbacks registered")
}

func (s *Server) handleSelf(w http.ResponseWriter, r *http.Request) {
	// Handle POST requests (modify self info)
	if r.Method == http.MethodPost {
		params, err := getRequestParams(r)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		// Update name
		if newName := params.Get("name"); newName != "" {
			if err := s.tox.SelfSetName(newName); err != nil {
				http.Error(w, fmt.Sprintf(`{"error":"failed to set name: %s"}`, err), http.StatusBadRequest)
				return
			}
			log.Printf("[TOX] Self name updated: %s", newName)
		}

		// Update status message
		if newStatus := params.Get("status_message"); newStatus != "" {
			if _, err := s.tox.SelfSetStatusMessage(newStatus); err != nil {
				http.Error(w, fmt.Sprintf(`{"error":"failed to set status: %s"}`, err), http.StatusBadRequest)
				return
			}
			log.Printf("[TOX] Self status message updated: %s", newStatus)
		}

		// Return updated info
		addr := s.tox.SelfGetAddress()
		name := s.tox.SelfGetName()
		status, _ := s.tox.SelfGetStatusMessage()

		s.mu.RLock()
		connStatus := s.selfConnectionStatus
		s.mu.RUnlock()

		resp := map[string]interface{}{
			"message":           "updated",
			"address":           addr,
			"name":              name,
			"status_message":    status,
			"connection_status": connStatus,
		}
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(resp)
		return
	}

	// Handle GET requests (query self info)
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
		params, err := getRequestParams(r)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}
		pubkeyStr := params.Get("public_key")
		message := params.Get("message")
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
		params, err := getRequestParams(r)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}
		friendIDStr := params.Get("friend_id")
		var friendID uint32
		fmt.Sscanf(friendIDStr, "%d", &friendID)

		_, err = s.tox.FriendDelete(friendID)
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

	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	friendIDStr := params.Get("friend_id")
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

	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	friendIDStr := params.Get("friend_id")
	message := params.Get("message")

	var friendID uint32
	fmt.Sscanf(friendIDStr, "%d", &friendID)

	msgID, err := s.tox.FriendSendMessage(friendID, message)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	// Persist outgoing friend message to SQLite with integer IDs
	friendPubKey, _ := s.tox.FriendGetPublicKey(friendID)
	chanidInt, _ := s.getOrCreatePubKeyID(friendPubKey)
	selfPubKey := s.tox.SelfGetPublicKey()
	senderInt, _ := s.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	s.persistEventToSQLite(chanidInt, string(data))

	resp := map[string]interface{}{
		"message_id": msgID,
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleGroups(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		// 获取所有群组列表（使用新 Group API）
		numGroups := s.tox.GroupGetNumberGroups()
		// 构建包含 group_number, group_name, chat_id 的对象数组
		groups := make([]map[string]interface{}, 0, numGroups)
		for i := uint32(0); i < numGroups; i++ {
			gn := tox.GroupNumber(i) // 类型转换
			group := map[string]interface{}{
				"group_number": gn,
				"group_name":   "",
				"chat_id":      "",
				"is_connected": false, // 新增
			}
			// 获取群组名称
			if name, err := s.tox.GroupGetName(gn); err == nil {
				group["group_name"] = name
			}
			// 获取群组 chat_id (public key)
			if chatId, err := s.tox.GroupGetChatId(gn); err == nil {
				group["chat_id"] = chatId
			}
			// 获取群组连接状态
			if connected, err := s.tox.GroupIsConnected(gn); err == nil {
				group["is_connected"] = connected
			}
			groups = append(groups, group)
		}
		resp := map[string]interface{}{
			"groups": groups,
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
		params, err := getRequestParams(r)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		privacyStateStr := params.Get("privacy_state")
		groupName := params.Get("group_name")
		name := params.Get("name") // 创建者昵称（C 函数第5个参数）
		password := params.Get("password")

		if groupName == "" {
			http.Error(w, `{"error":"missing group_name"}`, http.StatusBadRequest)
			return
		}

		var privacyState tox.GroupPrivacyState
		if privacyStateStr == "private" {
			privacyState = tox.GroupPrivacyState(tox.GROUP_PRIVACY_STATE_PRIVATE)
		} else {
			privacyState = tox.GroupPrivacyState(tox.GROUP_PRIVACY_STATE_PUBLIC)
		}

		// C 函数第5个参数是创建者昵称（name），不是密码
		groupNumber, err := s.tox.GroupNew(privacyState, groupName, name)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		// 如果提供了密码，在创建后设置
		if password != "" {
			err = s.tox.GroupSetPassword(groupNumber, password)
			if err != nil {
				http.Error(w, fmt.Sprintf(`{"error":"set password failed: %s"}`, err), http.StatusBadRequest)
				return
			}
		}

		resp := map[string]interface{}{
			"group_number": uint32(groupNumber),
			"message": "group created",
		}
		json.NewEncoder(w).Encode(resp)
	}
}

func (s *Server) handleConferences(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		// Use ConferenceGetChatlist() from group.go to get all conferences
		confIDs := s.tox.ConferenceGetChatlist()
		// Build conference objects with names, chat_id, and connected state
		conferences := make([]map[string]interface{}, 0, len(confIDs))
		for _, confID := range confIDs {
			conf := map[string]interface{}{
				"conference_number": confID,
				"conference_name":   "",
				"chat_id":           "",
				"is_connected":      false,
			}
			// Try to get conference title
			if title, err := s.tox.ConferenceGetTitle(confID); err == nil {
				conf["conference_name"] = title
			}
			// Get conference identifier (public key)
			if chatId, err := s.tox.ConferenceGetIdentifier(confID); err == nil {
				conf["chat_id"] = chatId
			}
			// Get connected state from map
			s.mu.RLock()
			if connected, ok := s.conferenceConnected[confID]; ok {
				conf["is_connected"] = connected
			}
			s.mu.RUnlock()
			conferences = append(conferences, conf)
		}
		resp := map[string]interface{}{
			"conferences": conferences,
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
		// Create new conference using ConferenceNew() from group.go
		confID, err := s.tox.ConferenceNew()
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}
		resp := map[string]interface{}{
			"conference_id": confID,
			"message":       "Conference created successfully",
		}
		json.NewEncoder(w).Encode(resp)
	}
}

func (s *Server) handleGroupSendMessage(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	groupNumberStr := params.Get("group_number")
	messageTypeStr := params.Get("message_type")
	message := params.Get("message")

	if groupNumberStr == "" || message == "" {
		http.Error(w, `{"error":"missing required parameters"}`, http.StatusBadRequest)
		return
	}

	var groupNumber tox.GroupNumber
	fmt.Sscanf(groupNumberStr, "%d", &groupNumber)

	messageType := tox.MESSAGE_TYPE_NORMAL
	if messageTypeStr == "action" {
		messageType = tox.MESSAGE_TYPE_ACTION
	}

	msgId, err := s.tox.GroupSendMessage(groupNumber, messageType, message)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	// Persist outgoing group message to SQLite with integer IDs
	chatId, _ := s.tox.GroupGetChatId(groupNumber)
	chanidInt, _ := s.getOrCreatePubKeyID(chatId)
	selfPubKey := s.tox.SelfGetPublicKey()
	senderInt, _ := s.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	if true { // dup with callback msg
		s.persistEventToSQLite(chanidInt, string(data))
	}

	resp := map[string]interface{}{
		"message_id": uint64(msgId),
		"message":   "message sent",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleConferenceMessages(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	confIDStr := params.Get("conference_id")
	message := params.Get("message")

	var confID uint32
	fmt.Sscanf(confIDStr, "%d", &confID)

	_, err = s.tox.ConferenceSendMessage(confID, tox.MESSAGE_TYPE_NORMAL, message)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	// Persist outgoing conference message to SQLite with integer IDs
	chatId, _ := s.tox.ConferenceGetIdentifier(confID)
	chanidInt, _ := s.getOrCreatePubKeyID(chatId)
	selfPubKey := s.tox.SelfGetPublicKey()
	senderInt, _ := s.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	if false { // dup with callback msg
		s.persistEventToSQLite(chanidInt, string(data))
	}
	
	resp := map[string]interface{}{
		"conference_id": confID,
		"message":       "sent",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleConferenceJoin(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	friendNumberStr := params.Get("friend_number")
	cookie := params.Get("cookie")

	var friendNumber uint32
	fmt.Sscanf(friendNumberStr, "%d", &friendNumber)

	if cookie == "" {
		http.Error(w, `{"error":"missing cookie"}`, http.StatusBadRequest)
		return
	}

	confID, err := s.tox.ConferenceJoin(friendNumber, cookie)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	log.Printf("[TOX] Successfully joined conference %d from friend %d", confID, friendNumber)

	go saveToxData(s.tox, "data/savedata.bin")

	resp := map[string]interface{}{
		"conference_id": confID,
		"message":       "Successfully joined conference",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleConferenceReject(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	friendNumberStr := params.Get("friend_number")

	var friendNumber uint32
	fmt.Sscanf(friendNumberStr, "%d", &friendNumber)

	log.Printf("[TOX] Rejected conference invite from friend %d", friendNumber)

	resp := map[string]interface{}{
		"message": "Conference invite rejected",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleConferenceIgnore(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	log.Printf("[TOX] Ignored conference invite (no action taken)")

	resp := map[string]interface{}{
		"message": "Conference invite ignored",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleGroupJoin(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	chatId := params.Get("chat_id")
	name := params.Get("name")
	password := params.Get("password")

	if chatId == "" {
		http.Error(w, `{"error":"missing chat_id"}`, http.StatusBadRequest)
		return
	}

	// 如果 name 为空，自动填充
	if name == "" {
		// 尝试使用自己的昵称
		name = s.tox.SelfGetName()

		// 如果昵称也为空，使用 "nonamed" + pubkey前5个字符
		if name == "" {
			pubkey := s.tox.SelfGetPublicKey() // 返回64字符hex大写
			if len(pubkey) >= 5 {
				name = "nonamed" + pubkey[:5]
			} else {
				name = "nonamed"
			}
		}
		log.Printf("[GroupJoin] name auto-filled: %s", name)
	}

	// 验证 chat_id 长度
	if len(chatId) != 64 {
		http.Error(w, fmt.Sprintf(`{"error":"invalid chat_id length: expected 64, got %d"}`, len(chatId)), http.StatusBadRequest)
		return
	}

	// 验证是否为有效的十六进制字符串
	if !regexp.MustCompile(`^[0-9a-fA-F]{64}$`).MatchString(chatId) {
		http.Error(w, `{"error":"invalid chat_id format: must be 64 hex characters (0-9, A-F)"}`, http.StatusBadRequest)
		return
	}

	groupNumber, err := s.tox.GroupJoin(chatId, name, password)
	if err != nil {
		errStr := err.Error()
		// 提供更友好的错误信息
		if strings.Contains(errStr, "chat id invalid") || strings.Contains(errStr, "3") {
			http.Error(w, `{"error":"无效的群组ID：群组不存在、已过期或需要密码"}`, http.StatusBadRequest)
		} else if strings.Contains(errStr, "bad password") {
			http.Error(w, `{"error":"密码错误"}`, http.StatusBadRequest)
		} else if strings.Contains(errStr, "failed to decrypt") {
			http.Error(w, `{"error":"群组ID解密失败，可能已损坏"}`, http.StatusBadRequest)
		} else {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, errStr), http.StatusBadRequest)
		}
		return
	}

	resp := map[string]interface{}{
		"group_number": uint32(groupNumber),
		"message":      "group joined",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleBootstrap(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	log.Println("Bootstrap requested")
	// Use bootstrap nodes from bootstrap.json (auto bootstrap all nodes)
	bootstrapAll(s.tox)

	resp := map[string]interface{}{
		"message": "bootstrap initiated to 3 nodes",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleGroupLeave(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	groupNumberStr := params.Get("group_number")
	partMessage := params.Get("part_message")

	if groupNumberStr == "" {
		http.Error(w, `{"error":"missing group_number"}`, http.StatusBadRequest)
		return
	}

	var groupNumber tox.GroupNumber
	fmt.Sscanf(groupNumberStr, "%d", &groupNumber)

	err = s.tox.GroupLeave(groupNumber, partMessage)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	resp := map[string]interface{}{
		"message": "group left",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
	}

func (s *Server) handleGroupInvite(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	groupNumberStr := params.Get("group_number")
	friendNumberStr := params.Get("friend_number")

	if groupNumberStr == "" || friendNumberStr == "" {
		http.Error(w, `{"error":"missing required parameters"}`, http.StatusBadRequest)
		return
	}

	var groupNumber tox.GroupNumber
	var friendNumber uint32
	fmt.Sscanf(groupNumberStr, "%d", &groupNumber)
	fmt.Sscanf(friendNumberStr, "%d", &friendNumber)

	err = s.tox.GroupInviteFriend(groupNumber, friendNumber)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	resp := map[string]interface{}{
		"message": "invite sent",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
	}

func (s *Server) handleGroupAccept(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	inviteData := params.Get("invite_data")
	friendNumberStr := params.Get("friend_number")
	name := params.Get("name")
	password := params.Get("password")

	if inviteData == "" || friendNumberStr == "" {
		http.Error(w, `{"error":"missing required parameters"}`, http.StatusBadRequest)
		return
	}

	// 确保 name 不为空（Tox Core 要求 name_length > 0）
	name = s.getDefaultName(name)

	var friendNumber uint32
	fmt.Sscanf(friendNumberStr, "%d", &friendNumber)

	groupNumber, err := s.tox.GroupInviteAccept(inviteData, friendNumber, name, password)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}

	resp := map[string]interface{}{
		"group_number": uint32(groupNumber),
		"message":      "invite accepted",
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleEvents(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodDelete {
		// Delete event by ID
		eventIDStr := r.URL.Query().Get("id")
		var eventID uint64
		fmt.Sscanf(eventIDStr, "%d", &eventID)
		if eventID > 0 {
			s.eventQueue.DeleteEvent(eventID)
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(map[string]string{"message": "event deleted"})
			return
		}
		http.Error(w, `{"error":"invalid event id"}`, http.StatusBadRequest)
		return
	}

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
	http.HandleFunc("/api/conference_messages", loggingMiddleware(s.handleConferenceMessages))
	http.HandleFunc("/api/conferences/join", loggingMiddleware(s.handleConferenceJoin))
	http.HandleFunc("/api/conferences/reject", loggingMiddleware(s.handleConferenceReject))
	http.HandleFunc("/api/conferences/ignore", loggingMiddleware(s.handleConferenceIgnore))
	http.HandleFunc("/api/bootstrap", loggingMiddleware(s.handleBootstrap))
	http.HandleFunc("/api/events", loggingMiddleware(s.handleEvents))
	http.HandleFunc("/api/messages/history", loggingMiddleware(s.handleMessageHistory))
	http.HandleFunc("/", loggingMiddleware(s.handleWeb))

	// Group Chat API 路由 (复数形式 /api/groups)
	http.HandleFunc("/api/groups/join", loggingMiddleware(s.handleGroupJoin))
	http.HandleFunc("/api/groups/leave", loggingMiddleware(s.handleGroupLeave))
	http.HandleFunc("/api/group_messages", loggingMiddleware(s.handleGroupSendMessage))
	http.HandleFunc("/api/groups/invite", loggingMiddleware(s.handleGroupInvite))
	http.HandleFunc("/api/groups/accept", loggingMiddleware(s.handleGroupAccept))

	// 成员列表 API 路由
	http.HandleFunc("/api/conference/members", loggingMiddleware(s.handleConferenceMembers))
	http.HandleFunc("/api/group/members", loggingMiddleware(s.handleGroupMembers))

	log.Printf("Server starting on :%s", port)
	return http.ListenAndServe(":"+port, nil)
}

func main() {
	// Command line flags
	udpEnabled := flag.Bool("udp", false, "Enable UDP mode (default: TCP only)")
	port := flag.String("port", "8181", "HTTP server port")
	debugLevel := flag.String("debug", "info", "Log level: trace, debug, info, warn, error")
	flag.Parse()

	// Set log level based on -debug flag
	setLogLevel(*debugLevel)

	server, err := NewServer(*udpEnabled)
	if err != nil {
		log.Fatalf("Failed to create server: %v", err)
	}

	// Add test data if no friends/groups exist
	addTestData(server)

	// Bootstrap to Tox network using C version nodes
	log.Println("[TOX] Starting bootstrap to 3 nodes...")
	bootstrapAll(server.tox)
	log.Println("[TOX] Bootstrap completed")

	// Log initial Tox status
	logToxStatus(server.tox)

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
		// Save Tox data
		saveToxData(server.tox, "data/savedata.bin")
		server.tox.Kill()
		os.Exit(0)
	}()

	log.Printf("[TOX] UDP enabled: %v", *udpEnabled)
	log.Fatal(server.Start(*port))
}

// logToxStatus logs the current status of the Tox instance
func logToxStatus(t *tox.Tox) {
	addr := t.SelfGetAddress()
	name := t.SelfGetName()
	connStatus := t.SelfGetConnectionStatus()
	var connStr string
	switch connStatus {
	case tox.CONNECTION_NONE:
		connStr = "OFFLINE"
	case tox.CONNECTION_TCP:
		connStr = "TCP"
	case tox.CONNECTION_UDP:
		connStr = "UDP"
	default:
		connStr = "UNKNOWN"
	}
	friends := t.SelfGetFriendList()
	log.Printf("[TOX] Status: name=%s, addr=%s, connection=%s (%d), friends=%d",
		name, addr[:16]+"...", connStr, connStatus, len(friends))
}

// saveToxData saves the Tox instance data to file
func saveToxData(t *tox.Tox, path string) {
	os.MkdirAll("data", 0700)
	
	// Use go-toxcore-c's WriteSavedata method
	err := t.WriteSavedata(path)
	if err != nil {
		log.Printf("[TOX] Failed to save data to %s: %v", path, err)
	} else {
		log.Printf("[TOX] Saved data to %s", path)
	}
}

// setLogLevel sets the log flags based on the debug level
func setLogLevel(level string) {
	// Set Go standard log flags
	switch level {
	case "trace", "debug":
		log.SetFlags(log.Lshortfile | log.LstdFlags)
	case "info":
		log.SetFlags(log.LstdFlags)
	case "warn", "error":
		log.SetFlags(log.LstdFlags)
	default:
		log.SetFlags(log.LstdFlags)
	}
	
	// Set toxcore log level (integer)
	var toxLevel int
	switch level {
	case "trace":
		toxLevel = tox.LOG_LEVEL_TRACE
	case "debug":
		toxLevel = tox.LOG_LEVEL_DEBUG
	case "info":
		toxLevel = tox.LOG_LEVEL_INFO
	case "warn":
		toxLevel = tox.LOG_LEVEL_WARNING
	case "error":
		toxLevel = tox.LOG_LEVEL_ERROR
	default:
		toxLevel = tox.LOG_LEVEL_INFO
	}
	tox.SetLogLevel(toxLevel)
	
	log.Printf("[MAIN] Log level set to: %s (toxcore level: %d)", level, toxLevel)
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

// handleConferenceMembers returns the list of peers in a conference
func (s *Server) handleConferenceMembers(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	confIDStr := r.URL.Query().Get("conference_id")
	if confIDStr == "" {
		http.Error(w, `{"error":"missing conference_id"}`, http.StatusBadRequest)
		return
	}

	var confID uint32
	fmt.Sscanf(confIDStr, "%d", &confID)

	// Get peer list using ConferenceGetPeers (returns map[peerNumber]pubkey)
	peers := s.tox.ConferenceGetPeers(confID)

	// Build member list with names
	members := make([]map[string]interface{}, 0, len(peers))
	for peerNumber := range peers {
		name, err := s.tox.ConferencePeerGetName(confID, peerNumber)
		if err != nil {
			name = fmt.Sprintf("Peer %d", peerNumber)
		}
		members = append(members, map[string]interface{}{
			"peer_number": int(peerNumber),
			"name":        name,
		})
	}

	resp := map[string]interface{}{
		"conference_id": int(confID),
		"members":       members,
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

// handleGroupMembers returns the list of peers in a group (NGC)
func (s *Server) handleGroupMembers(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	groupNumberStr := r.URL.Query().Get("group_number")
	if groupNumberStr == "" {
		http.Error(w, `{"error":"missing group_number"}`, http.StatusBadRequest)
		return
	}

	var groupNumber tox.GroupNumber
	fmt.Sscanf(groupNumberStr, "%d", &groupNumber)

	// Iterate peer numbers 0-255 (reasonable upper limit for group size)
	members := make([]map[string]interface{}, 0)
	for peerNumber := 0; peerNumber < 256; peerNumber++ {
		name, err := s.tox.GroupPeerGetName(groupNumber, tox.GroupPeerNumber(peerNumber))
		if err != nil {
			// Peer doesn't exist, stop iterating
			break
		}
		members = append(members, map[string]interface{}{
			"peer_number": peerNumber,
			"name":        name,
		})
	}

	resp := map[string]interface{}{
		"group_number": int(groupNumber),
		"members":      members,
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) handleFriendDelete(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete && r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	friendIDStr := params.Get("friend_id")
	var friendID uint32
	fmt.Sscanf(friendIDStr, "%d", &friendID)

	_, err = s.tox.FriendDelete(friendID)
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

// getDefaultName returns a default name for group join.
// If input name is not empty, returns it directly.
// Otherwise uses self name if available, otherwise "nonamed." + first 7 chars of public key.
func (s *Server) getDefaultName(name string) string {
	if name != "" {
		return name
	}
	selfName := s.tox.SelfGetName()
	if selfName != "" {
		return selfName
	}
	pubkey := s.tox.SelfGetPublicKey()
	if len(pubkey) >= 7 {
		return "nonamed." + pubkey[:7]
	}
	return "nonamed"
}

// getContactNumber returns the contact number (friend number or peer number) for a given public key
// contactType: "friend", "group", or "conference"
// contactPubkey: the contact's public key (chatId for groups, identifier for conferences)
// senderPubkey: the sender's public key to look up
// Returns uint32 contact number, or ContactNotFound (0xFFFFFFFF) if not found
func (s *Server) getContactNumber(contactType string, contactPubkey string, senderPubkey string) (uint32, error) {
	switch contactType {
	case "friend":
		num, err := s.tox.FriendByPublicKey(senderPubkey)
		if err != nil {
			return ContactNotFound, nil
		}
		return num, nil

	case "group":
		// For groups (NGC), use GroupByChatId to get group number, then iterate peers
		groupNum, err := s.tox.GroupByChatId(contactPubkey)
		if err != nil {
			return ContactNotFound, nil
		}
		// Iterate through peer list (0-99 should be enough for most groups)
		for i := uint32(0); i < 100; i++ {
			peerPubkey, err := s.tox.GroupPeerGetPublicKey(groupNum, tox.GroupPeerNumber(i))
			if err != nil {
				continue
			}
			if strings.EqualFold(peerPubkey, senderPubkey) {
				return i, nil
			}
		}
		return ContactNotFound, nil

	case "conference":
		// For conferences (legacy), iterate through all conferences to find matching identifier
		chatlist := s.tox.ConferenceGetChatlist()
		for _, confNum := range chatlist {
			identifier, err := s.tox.ConferenceGetIdentifier(confNum)
			if err != nil {
				continue
			}
			if strings.EqualFold(identifier, contactPubkey) {
				// Found the conference, now iterate through its peers
				for j := uint32(0); j < 100; j++ {
					peerPubkey, err := s.tox.ConferencePeerGetPublicKey(confNum, j)
					if err != nil {
						continue
					}
					if strings.EqualFold(peerPubkey, senderPubkey) {
						return j, nil
					}
				}
				return ContactNotFound, nil
			}
		}
		return ContactNotFound, nil
	}
	return ContactNotFound, fmt.Errorf("unknown contact type: %s", contactType)
}

// handleMessageHistory returns message history for a channel
// Query params: chanid (pubkey) or contact_id (numeric id) + contact_type (friend|group|conference)
func (s *Server) handleMessageHistory(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	chanid := r.URL.Query().Get("chanid")       // contact's pubkey
	contactIDStr := r.URL.Query().Get("contact_id")
	contactType := r.URL.Query().Get("contact_type")

	if contactType == "" {
		http.Error(w, `{"error":"missing contact_type"}`, http.StatusBadRequest)
		return
	}

	// If contact_id is provided, resolve to pubkey (chanid)
	if contactIDStr != "" {
		id, err := strconv.ParseUint(contactIDStr, 10, 32)
		if err != nil {
			http.Error(w, `{"error":"invalid contact_id"}`, http.StatusBadRequest)
			return
		}
		switch contactType {
		case "friend":
			chanid, err = s.tox.FriendGetPublicKey(uint32(id))
		case "group":
			chanid, err = s.tox.GroupGetChatId(tox.GroupNumber(id))
		case "conference":
			chanid, err = s.tox.ConferenceGetIdentifier(uint32(id))
		default:
			http.Error(w, `{"error":"unknown contact_type"}`, http.StatusBadRequest)
			return
		}
		if err != nil {
			http.Error(w, `{"error":"failed to resolve contact_id"}`, http.StatusBadRequest)
			return
		}
	}

	if chanid == "" {
		http.Error(w, `{"error":"missing chanid or contact_id"}`, http.StatusBadRequest)
		return
	}

	// Convert chanid (pubkey) to integer ID for database query
	var chanidInt int64
	err := s.db.QueryRow("SELECT pkid FROM pubkey_ids WHERE pubkey = ?", chanid).Scan(&chanidInt)
	if err != nil {
		http.Error(w, `{"error":"invalid chanid"}`, http.StatusBadRequest)
		return
	}

	// Query latest 50 messages, descending (newest first)
	rows, err := s.db.Query(`SELECT rowid, data, created_at FROM events WHERE chanid = ? ORDER BY rowid DESC LIMIT 50`, chanidInt)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}
	defer rows.Close()

	messages := []map[string]interface{}{}
	for rows.Next() {
		var rowid int64
		var dataStr string
		var createdAt string
		if err := rows.Scan(&rowid, &dataStr, &createdAt); err != nil {
			continue
		}

		var eventData map[string]interface{}
		if err := json.Unmarshal([]byte(dataStr), &eventData); err != nil {
			continue
		}

		// Get sender pubkey from sender ID
		senderPubkey := ""
		if sender, ok := eventData["sender"].(float64); ok {
			var pk string
			if err := s.db.QueryRow("SELECT pubkey FROM pubkey_ids WHERE pkid = ?", int64(sender)).Scan(&pk); err == nil {
				senderPubkey = pk
			}
		}

		// Get sender_number using getContactNumber
		senderNumber := uint32(ContactNotFound)
		if senderPubkey != "" {
			senderNumber, _ = s.getContactNumber(contactType, chanid, senderPubkey)
		}

		messages = append(messages, map[string]interface{}{
			"rowid":         rowid,
			"message":       eventData["message"],
			"sender_pubkey": senderPubkey,
			"sender_number": senderNumber,
			"direction":     eventData["direction"],
			"created_at":    createdAt,
		})
	}

	// Reverse array: DESC (newest first) -> ASC (oldest first) for display
	for i, j := 0, len(messages)-1; i < j; i, j = i+1, j-1 {
		messages[i], messages[j] = messages[j], messages[i]
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"messages": messages,
	})
}
