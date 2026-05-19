package server

import (
	"context"
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"

	tox "github.com/TokTok/go-toxcore-c"
	"github.com/envsh/toxera/toxpriv"
)

type Server struct {
	tox                  *tox.Tox
	toxp                 *toxpriv.Tox
	db                   *sql.DB
	eventQueue           *EventQueue
	selfConnectionStatus string
	friendStatuses       map[uint32]string
	friendUserStatuses   map[uint32]int
	conferenceConnected  map[uint32]bool
	mu                   sync.RWMutex
	webRoot              string
	config               Config
	rebscnter            int
	shutdownCh           chan struct{}
	httpServer           *http.Server
}

func New(cfg Config) (*Server, error) {
	saveDataPath := "data/savedata.bin"
	var saveData []byte
	if data, err := os.ReadFile(saveDataPath); err == nil && len(data) > 100 {
		saveData = data
		log.Printf("[TOX] Loaded save data from %s (%d bytes)", saveDataPath, len(data))
	} else {
		log.Printf("[TOX] No save data found, creating new identity")
	}

	opts := tox.NewToxOptions()
	opts.Udp_enabled = cfg.UDPEnabled
	opts.GroupsPersistence = true
	if saveData != nil {
		opts.Savedata_type = tox.SAVEDATA_TYPE_TOX_SAVE
		opts.Savedata_data = saveData
	}
	t := tox.NewTox(opts)
	if t == nil {
		return nil, fmt.Errorf("failed to create tox instance")
	}

	db, err := initMsgHistDB("data/msghist.db")
	if err != nil {
		return nil, fmt.Errorf("failed to init msghist db: %w", err)
	}

	// Determine web root
	webRoot := cfg.WebRoot
	if webRoot == "" {
		wr, err := detectWebRoot()
		if err != nil {
			return nil, fmt.Errorf("web root detection failed: %w", err)
		}
		webRoot = wr
	}

	server := &Server{
		tox:                  t,
		toxp:                 toxpriv.NewTox(getInnerPtr(t)),
		db:                   db,
		eventQueue:           NewEventQueue(),
		selfConnectionStatus: "offline",
		friendStatuses:       make(map[uint32]string),
		friendUserStatuses:   make(map[uint32]int),
		conferenceConnected:  make(map[uint32]bool),
		webRoot:              webRoot,
		config:               cfg,
		shutdownCh:           make(chan struct{}),
	}

	setupCallbacks(server)

	// Add test data / log identity
	addTestData(server)

	// Bootstrap to Tox network
	log.Println("[TOX] Starting bootstrap to 3 nodes...")
	bootstrapAll(server.tox)
	log.Println("[TOX] Bootstrap completed")
	logToxStatus(server.tox)

	log.Printf("[TOX] UDP enabled: %v", cfg.UDPEnabled)

	return server, nil
}

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

func setupCallbacks(s *Server) {
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

	s.tox.CallbackFriendRequest(func(this *tox.Tox, pubkey string, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendRequest: pubkey=%s, message=%s", pubkey, message)
		_, err := s.tox.FriendAddNorequest(pubkey)
		if err != nil {
			log.Printf("Failed to accept friend request: %v", err)
		} else {
			data, _ := json.Marshal(map[string]interface{}{"public_key": pubkey})
			s.eventQueue.Push("friend_request", string(data))
		}
	}, nil)

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
		s.persistEventToSQLite(chanidInt, string(data))
	}, nil)

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

	s.tox.CallbackFriendName(func(this *tox.Tox, friendNumber uint32, newName string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendName: friend=%d, name=%s", friendNumber, newName)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"name":      newName,
		})
		s.eventQueue.Push("friend_name", string(data))
	}, nil)

	s.tox.CallbackFriendStatusMessage(func(this *tox.Tox, friendNumber uint32, newStatus string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendStatusMessage: friend=%d, status=%s", friendNumber, newStatus)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id":      friendNumber,
			"status_message": newStatus,
		})
		s.eventQueue.Push("friend_status_message", string(data))
	}, nil)

	s.tox.CallbackFriendStatus(func(this *tox.Tox, friendNumber uint32, status int, userData interface{}) {
		s.mu.Lock()
		s.friendUserStatuses[friendNumber] = status
		s.mu.Unlock()
		log.Printf("[TOX_CALLBACK] FriendStatus: friend=%d, status=%d", friendNumber, status)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"status":    status,
		})
		s.eventQueue.Push("friend_status", string(data))
	}, nil)

	s.tox.CallbackFriendTyping(func(this *tox.Tox, friendNumber uint32, isTyping uint8, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendTyping: friend=%d, typing=%v", friendNumber, isTyping)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"typing":    isTyping,
		})
		s.eventQueue.Push("friend_typing", string(data))
	}, nil)

	s.tox.CallbackFriendReadReceipt(func(this *tox.Tox, friendNumber uint32, receipt uint32, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendReadReceipt: friend=%d, receipt=%d", friendNumber, receipt)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"receipt":   receipt,
		})
		s.eventQueue.Push("friend_read_receipt", string(data))
	}, nil)

	s.tox.CallbackFileRecvControl(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, control int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecvControl: friend=%d, file=%d, control=%d", friendNumber, fileNumber, control)
	}, nil)

	s.tox.CallbackFileRecv(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, kind uint32, fileSize uint64, fileName string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecv: friend=%d, file=%d, kind=%d, size=%d, name=%s", friendNumber, fileNumber, kind, fileSize, fileName)
	}, nil)

	s.tox.CallbackFileRecvChunk(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, position uint64, data []byte, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecvChunk: friend=%d, file=%d, position=%d, len=%d", friendNumber, fileNumber, position, len(data))
	}, nil)

	s.tox.CallbackFileChunkRequest(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, position uint64, length int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileChunkRequest: friend=%d, file=%d, position=%d, length=%d", friendNumber, fileNumber, position, length)
	}, nil)

	s.tox.CallbackConferenceInvite(func(this *tox.Tox, friendNumber uint32, itype uint8, cookie string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceInvite: friend=%d, type=%d, cookie=%s", friendNumber, itype, cookie)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_number": friendNumber,
			"type":          itype,
			"cookie":        cookie,
		})
		s.eventQueue.Push("conference_invite", string(data))
	}, nil)

	s.tox.CallbackConferenceMessage(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceMessage: group=%d, peer=%d, message=%s", groupNumber, peerNumber, message)
		chatId, _ := s.tox.ConferenceGetIdentifier(groupNumber)
		chanidInt, _ := s.getOrCreatePubKeyID(chatId)
		peerPubKey, _ := s.tox.ConferencePeerGetPublicKey(groupNumber, peerNumber)
		senderInt, _ := s.getOrCreatePubKeyID(peerPubKey)
		peerName, _ := s.tox.ConferencePeerGetName(groupNumber, peerNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"message":           message,
			"sender":            senderInt,
			"direction":         "received",
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
			"peer_name":         peerName,
		})
		s.eventQueue.Push("conference_message", string(data))
		s.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	s.tox.CallbackGroupMessage(func(this *tox.Tox, groupNumber tox.GroupNumber, peerNumber tox.GroupPeerNumber, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] GroupMessage: group=%d, peer=%d, message=%s", groupNumber, peerNumber, message)
		chatId, _ := s.tox.GroupGetChatId(groupNumber)
		chanidInt, _ := s.getOrCreatePubKeyID(chatId)
		peerPubKey, _ := s.tox.GroupPeerGetPublicKey(groupNumber, peerNumber)
		senderInt, _ := s.getOrCreatePubKeyID(peerPubKey)
		peerName, _ := s.tox.GroupPeerGetName(groupNumber, peerNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"message":      message,
			"sender":       senderInt,
			"direction":    "received",
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"peer_name":    peerName,
		})
		s.eventQueue.Push("group_message", string(data))
		s.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	s.tox.CallbackConferenceTitle(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, title string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceTitle: group=%d, peer=%d, title=%s", groupNumber, peerNumber, title)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
			"title":             title,
		})
		s.eventQueue.Push("conference_title", string(data))
	}, nil)

	s.tox.CallbackConferencePeerName(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, name string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferencePeerName: group=%d, peer=%d, name=%s", groupNumber, peerNumber, name)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
			"name":              name,
		})
		s.eventQueue.Push("conference_peer_name", string(data))
	}, nil)

	s.tox.CallbackConferencePeerListChanged(func(this *tox.Tox, groupNumber uint32, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferencePeerListChanged: group=%d", groupNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
		})
		s.eventQueue.Push("conference_peer_list_changed", string(data))
	}, nil)

	s.tox.CallbackGroupInvite(func(this *tox.Tox, groupNumber tox.GroupNumber,
		friendNumber uint32, data string, userData interface{}) {
		log.Printf("[GroupInvite] group=%d, friend=%d, data=%s",
			groupNumber, friendNumber, data)
		if data == "" {
			log.Printf("[GroupInvite] WARNING: empty invite data from friend %d, skipping event", friendNumber)
			return
		}
		eventData, _ := json.Marshal(map[string]interface{}{
			"friend_number": friendNumber,
			"chat_id":       data,
		})
		s.eventQueue.Push("group_invite", string(eventData))
		log.Printf("[GroupInvite] Pushed event: friend=%d, chat_id=%s", friendNumber, data)
	}, nil)

	s.tox.CallbackGroupSelfJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		userData interface{}) {
		log.Printf("[GroupSelfJoin] group=%d", groupNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": groupNumber,
		})
		s.eventQueue.Push("group_self_join", string(data))
	}, nil)

	s.tox.CallbackGroupPeerJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, userData interface{}) {
		log.Printf("[GroupPeerJoin] group=%d, peer=%d", int(groupNumber), int(peerNumber))
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
		})
		s.eventQueue.Push("group_peer_join", string(data))
	}, nil)

	s.tox.CallbackGroupPeerExit(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, exitType tox.GroupExitType, name string, userData interface{}) {
		log.Printf("[GroupPeerExit] group=%d, peer=%d, type=%s, name=%s",
			int(groupNumber), int(peerNumber), tox.GroupExitTypeToString(exitType), name)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"exit_type":    tox.GroupExitTypeToString(exitType),
			"name":         name,
		})
		s.eventQueue.Push("group_peer_exit", string(data))
	}, nil)

	s.tox.CallbackGroupPeerStatus(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, status int, userData interface{}) {
		log.Printf("[GroupPeerStatus] group=%d, peer=%d, status=%d", int(groupNumber), int(peerNumber), status)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"status":       status,
		})
		s.eventQueue.Push("group_peer_status", string(data))
	}, nil)

	s.tox.CallbackGroupJoinFail(func(this *tox.Tox, groupNumber tox.GroupNumber,
		failType tox.GroupJoinFail, userData interface{}) {
		failStr := tox.GroupJoinFailToString(failType)
		log.Printf("[GroupJoinFail] group=%d, error=%d (%s)",
			int(groupNumber), int(failType), failStr)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"error":        failStr,
		})
		s.eventQueue.Push("group_join_fail", string(data))
	}, nil)

	s.tox.CallbackGroupPeerName(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, name string, userData interface{}) {
		log.Printf("[GroupPeerName] group=%d, peer=%d, name=%s",
			int(groupNumber), int(peerNumber), name)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"name":         name,
		})
		s.eventQueue.Push("group_peer_name", string(data))
	}, nil)

	s.tox.CallbackConferenceConnected(func(this *tox.Tox, groupNumber uint32, userData interface{}) {
		log.Printf("[ConferenceConnected] group=%d", groupNumber)
		s.mu.Lock()
		s.conferenceConnected[groupNumber] = true
		s.mu.Unlock()
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"is_connected":      true,
		})
		s.eventQueue.Push("conference_connected", string(data))
	}, nil)

	log.Println("[TOX] All callbacks registered")
}

func (s *Server) Start() error {
	mux := http.NewServeMux()
	mux.HandleFunc("/api/self", corsMiddleware(loggingMiddleware(s.handleSelf)))
	mux.HandleFunc("/api/friends", corsMiddleware(loggingMiddleware(s.handleFriends)))
	mux.HandleFunc("/api/friend_delete", corsMiddleware(loggingMiddleware(s.handleFriendDelete)))
	mux.HandleFunc("/api/friend", corsMiddleware(loggingMiddleware(s.handleFriendInfo)))
	mux.HandleFunc("/api/messages", corsMiddleware(loggingMiddleware(s.handleSendMessage)))
	mux.HandleFunc("/api/groups", corsMiddleware(loggingMiddleware(s.handleGroups)))
	mux.HandleFunc("/api/conferences", corsMiddleware(loggingMiddleware(s.handleConferences)))
	mux.HandleFunc("/api/conference_messages", corsMiddleware(loggingMiddleware(s.handleConferenceMessages)))
	mux.HandleFunc("/api/conferences/join", corsMiddleware(loggingMiddleware(s.handleConferenceJoin)))
	mux.HandleFunc("/api/conferences/reject", corsMiddleware(loggingMiddleware(s.handleConferenceReject)))
	mux.HandleFunc("/api/conferences/ignore", corsMiddleware(loggingMiddleware(s.handleConferenceIgnore)))
	mux.HandleFunc("/api/bootstrap", corsMiddleware(loggingMiddleware(s.handleBootstrap)))
	mux.HandleFunc("/api/events", corsMiddleware(loggingMiddleware(s.handleEvents)))
	mux.HandleFunc("/api/messages/history", corsMiddleware(loggingMiddleware(s.handleMessageHistory)))
	mux.HandleFunc("/", corsMiddleware(loggingMiddleware(s.handleWeb)))
	mux.HandleFunc("/api/groups/join", corsMiddleware(loggingMiddleware(s.handleGroupJoin)))
	mux.HandleFunc("/api/groups/leave", corsMiddleware(loggingMiddleware(s.handleGroupLeave)))
	mux.HandleFunc("/api/group_messages", corsMiddleware(loggingMiddleware(s.handleGroupSendMessage)))
	mux.HandleFunc("/api/groups/invite", corsMiddleware(loggingMiddleware(s.handleGroupInvite)))
	mux.HandleFunc("/api/groups/accept", corsMiddleware(loggingMiddleware(s.handleGroupAccept)))
	mux.HandleFunc("/api/conference/members", corsMiddleware(loggingMiddleware(s.handleConferenceMembers)))
	mux.HandleFunc("/api/group/members", corsMiddleware(loggingMiddleware(s.handleGroupMembers)))
	mux.HandleFunc("/api/random-name", corsMiddleware(loggingMiddleware(s.handleRandomName)))
	mux.HandleFunc("/api/groups/set-name", corsMiddleware(loggingMiddleware(s.handleGroupSetName)))
	mux.HandleFunc("/api/translate", corsMiddleware(loggingMiddleware(s.handleTranslate)))

	// Start tox iteration in background
	ctx, cancel := context.WithCancel(context.Background())
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			select {
			case <-ctx.Done():
				return
			default:
				s.tox.Iterate()
				s.checkRebootstrap()
				time.Sleep(time.Millisecond * time.Duration(s.tox.IterationInterval()))
			}
		}
	}()

	s.httpServer = &http.Server{Addr: ":" + s.config.Port, Handler: mux}

	// HTTP server error channel
	httpErr := make(chan error, 1)
	go func() {
		log.Printf("Server starting on :%s", s.config.Port)
		if err := s.httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			httpErr <- err
		}
	}()

	// Wait for either HTTP error or shutdown signal
	select {
	case err := <-httpErr:
		cancel()
		wg.Wait()
		return err
	case <-s.shutdownCh:
		log.Println("Shutting down...")
		cancel()
		wg.Wait()
		saveToxData(s.tox, "data/savedata.bin")
		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer shutdownCancel()
		s.httpServer.Shutdown(shutdownCtx)
		s.tox.Kill()
		log.Println("Server stopped.")
		return nil
	}
}

func (s *Server) Shutdown() {
	close(s.shutdownCh)
}
