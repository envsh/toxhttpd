package main

import (
	"encoding/json"
	"flag"
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
	if saveData != nil {
		opts.Savedata_type = tox.SAVEDATA_TYPE_TOX_SAVE
		opts.Savedata_data = saveData
	}
	t := tox.NewTox(opts)
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
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"message":   message,
		})
		s.eventQueue.Push("friend_message", string(data))
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
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":      peerNumber,
			"message":          message,
		})
		s.eventQueue.Push("conference_message", string(data))
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

	// Group Chat callbacks (stubs)
	// 1. CallbackGroupChatInvite - 邀请回调
	s.tox.CallbackGroupChatInvite(func(this *tox.Tox, groupNumber tox.GroupNumber,
		friendNumber uint32, data string, userData interface{}) {
		log.Printf("[GroupInvite] group=%d, friend=%d, data=%s",
			groupNumber, friendNumber, data)
		// TODO: 处理邀请事件，可能触发 Web 前端通知
	}, nil)

	// 2. CallbackGroupPeerJoin - Peer 加入回调
	s.tox.CallbackGroupPeerJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, userData interface{}) {
		log.Printf("[GroupPeerJoin] group=%d, peer=%d",
			groupNumber, peerNumber)
		// TODO: 更新在线用户列表
	}, nil)

	// 3. CallbackGroupPeerExit - Peer 退出回调
	s.tox.CallbackGroupPeerExit(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, exitType tox.GroupExitType, name string, userData interface{}) {
		log.Printf("[GroupPeerExit] group=%d, peer=%d, type=%s, name=%s",
			groupNumber, peerNumber, tox.GroupExitTypeToString(exitType), name)
		// TODO: 更新在线用户列表
	}, nil)

	// 4. CallbackGroupPeerStatus - Peer 状态变化回调
	s.tox.CallbackGroupPeerStatus(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, status int, userData interface{}) {
		log.Printf("[GroupPeerStatus] group=%d, peer=%d, status=%d",
			groupNumber, peerNumber, status)
		// TODO: 更新前端状态显示
	}, nil)

	// 5. CallbackGroupSelfJoin - 自己加入群组回调
	s.tox.CallbackGroupSelfJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		userData interface{}) {
		log.Printf("[GroupSelfJoin] group=%d", groupNumber)
		// TODO: 初始化群组相关状态
	}, nil)

	log.Println("[TOX] All callbacks registered")
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
		// 获取所有群组列表（使用新 Group API）
		// GroupGetNumberGroups() 返回数量，需要生成 ID 数组 [0, 1, ..., n-1]
		numGroups := s.tox.GroupGetNumberGroups()
		groups := make([]uint32, 0, numGroups)
		for i := uint32(0); i < numGroups; i++ {
			groups = append(groups, i)
		}
		resp := map[string]interface{}{
			"groups": groups,
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
		r.ParseForm()

		privacyStateStr := r.FormValue("privacy_state")
		groupName := r.FormValue("group_name")
		name := r.FormValue("name") // 创建者昵称（C 函数第5个参数）
		password := r.FormValue("password")

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
		conferences := s.tox.ConferenceGetChatlist()
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
	r.ParseForm()

	groupNumberStr := r.FormValue("group_number")
	messageTypeStr := r.FormValue("message_type")
	message := r.FormValue("message")

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

	r.ParseForm()
	confIDStr := r.FormValue("conference_id")
	message := r.FormValue("message")

	var confID uint32
	fmt.Sscanf(confIDStr, "%d", &confID)

	_, err := s.tox.ConferenceSendMessage(confID, tox.MESSAGE_TYPE_NORMAL, message)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
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

	r.ParseForm()
	friendNumberStr := r.FormValue("friend_number")
	cookie := r.FormValue("cookie")

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

	r.ParseForm()
	friendNumberStr := r.FormValue("friend_number")

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
	r.ParseForm()

	chatId := r.FormValue("chat_id")
	name := r.FormValue("name")
	password := r.FormValue("password")

	if chatId == "" {
		http.Error(w, `{"error":"missing chat_id"}`, http.StatusBadRequest)
		return
	}

	groupNumber, err := s.tox.GroupJoin(chatId, name, password)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
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
	r.ParseForm()

	groupNumberStr := r.FormValue("group_number")
	partMessage := r.FormValue("part_message")

	if groupNumberStr == "" {
		http.Error(w, `{"error":"missing group_number"}`, http.StatusBadRequest)
		return
	}

	var groupNumber tox.GroupNumber
	fmt.Sscanf(groupNumberStr, "%d", &groupNumber)

	err := s.tox.GroupLeave(groupNumber, partMessage)
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
	r.ParseForm()

	groupNumberStr := r.FormValue("group_number")
	friendNumberStr := r.FormValue("friend_number")

	if groupNumberStr == "" || friendNumberStr == "" {
		http.Error(w, `{"error":"missing required parameters"}`, http.StatusBadRequest)
		return
	}

	var groupNumber tox.GroupNumber
	var friendNumber uint32
	fmt.Sscanf(groupNumberStr, "%d", &groupNumber)
	fmt.Sscanf(friendNumberStr, "%d", &friendNumber)

	err := s.tox.GroupInviteFriend(groupNumber, friendNumber)
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
	r.ParseForm()

	inviteData := r.FormValue("invite_data")
	friendNumberStr := r.FormValue("friend_number")
	name := r.FormValue("name")
	password := r.FormValue("password")

	if inviteData == "" || friendNumberStr == "" {
		http.Error(w, `{"error":"missing required parameters"}`, http.StatusBadRequest)
		return
	}

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
	http.HandleFunc("/", loggingMiddleware(s.handleWeb))

	// Group Chat API 路由 (复数形式 /api/groups)
	http.HandleFunc("/api/groups/join", loggingMiddleware(s.handleGroupJoin))
	http.HandleFunc("/api/groups/leave", loggingMiddleware(s.handleGroupLeave))
	http.HandleFunc("/api/group_messages", loggingMiddleware(s.handleGroupSendMessage))
	http.HandleFunc("/api/groups/invite", loggingMiddleware(s.handleGroupInvite))
	http.HandleFunc("/api/groups/accept", loggingMiddleware(s.handleGroupAccept))

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
