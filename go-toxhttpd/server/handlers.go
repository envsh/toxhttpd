package server

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	tox "github.com/TokTok/go-toxcore-c"
	"github.com/kitech/touse/oai"
)

const ContactNotFound = uint32(0xFFFFFFFF)

func (s *Server) handleSelf(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		params, err := getRequestParams(r)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		if newName := params.Get("name"); newName != "" {
			if err := s.tox.SelfSetName(newName); err != nil {
				http.Error(w, fmt.Sprintf(`{"error":"failed to set name: %s"}`, err), http.StatusBadRequest)
				return
			}
			log.Printf("[TOX] Self name updated: %s", newName)
		}

		if newStatus := params.Get("status_message"); newStatus != "" {
			if _, err := s.tox.SelfSetStatusMessage(newStatus); err != nil {
				http.Error(w, fmt.Sprintf(`{"error":"failed to set status: %s"}`, err), http.StatusBadRequest)
				return
			}
			log.Printf("[TOX] Self status message updated: %s", newStatus)
		}

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

	idsStr := params.Get("friend_ids")
	idStrs := strings.Split(idsStr, ",")
	friends := make([]map[string]interface{}, 0, len(idStrs))

	for _, idStr := range idStrs {
		idStr = strings.TrimSpace(idStr)

		entry := map[string]interface{}{
			"friendId": 0, "name": "", "iconUrl": "",
			"status": 0, "statusStr": "none",
			"statusText": "", "publicKey": "",
			"lastSeen": 0, "peerIp": nil,
			"error": "",
		}

		friendID, err := strconv.ParseUint(idStr, 10, 32)
		if err != nil {
			entry["error"] = "invalid friend id"
			friends = append(friends, entry)
			continue
		}
		fid := uint32(friendID)
		entry["friendId"] = fid

		name, err := s.tox.FriendGetName(fid)
		if err != nil {
			entry["error"] = err.Error()
			friends = append(friends, entry)
			continue
		}

		pk, _ := s.tox.FriendGetPublicKey(fid)
		statusText, _ := s.tox.FriendGetStatusMessage(fid)
		lastSeen, _ := s.tox.FriendGetLastOnline(fid)

		s.mu.RLock()
		connStatus := s.friendStatuses[fid]
		s.mu.RUnlock()

		statusInt := 0
		switch connStatus {
		case "tcp":
			statusInt = 1
		case "udp":
			statusInt = 2
		}

		s.mu.RLock()
		userStatus := s.friendUserStatuses[fid]
		s.mu.RUnlock()

		entry["name"] = name
		entry["status"] = statusInt
		entry["statusStr"] = statusToStr(statusInt)
		entry["userStatus"] = userStatus
		entry["statusText"] = statusText
		entry["publicKey"] = pk
		entry["lastSeen"] = lastSeen

		ip, ipErr := s.toxp.FriendGetConnectionIP(fid)
		if ipErr == nil {
			entry["peerIp"] = ip
		}

		friends = append(friends, entry)
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(friends)
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
		numGroups := s.tox.GroupGetNumberGroups()
		groups := make([]map[string]interface{}, 0, numGroups)
		for i := uint32(0); i < numGroups; i++ {
			gn := tox.GroupNumber(i)
			group := map[string]interface{}{
				"groupNumber":  gn,
				"groupName":    "",
				"chatId":       "",
				"isConnected":  false,
				"statusText":   "",
			}
			if name, err := s.tox.GroupGetName(gn); err == nil {
				group["groupName"] = name
			}
			if chatId, err := s.tox.GroupGetChatId(gn); err == nil {
				group["chatId"] = chatId
			}
			if connected, err := s.tox.GroupIsConnected(gn); err == nil {
				group["isConnected"] = connected
			}
			if topic, err := s.tox.GroupGetTopic(gn); err == nil {
				group["statusText"] = topic
			}
			group["memberCount"] = s.groupPeerCount(gn)
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
		name := params.Get("name")
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

		groupNumber, err := s.tox.GroupNew(privacyState, groupName, name)
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
			return
		}

		if password != "" {
			err = s.tox.GroupSetPassword(groupNumber, password)
			if err != nil {
				http.Error(w, fmt.Sprintf(`{"error":"set password failed: %s"}`, err), http.StatusBadRequest)
				return
			}
		}

		resp := map[string]interface{}{
			"group_number": uint32(groupNumber),
			"message":      "group created",
		}
		json.NewEncoder(w).Encode(resp)
	}
}

func (s *Server) handleConferences(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		confIDs := s.tox.ConferenceGetChatlist()
		conferences := make([]map[string]interface{}, 0, len(confIDs))
		for _, confID := range confIDs {
			conf := map[string]interface{}{
				"conferenceNumber": confID,
				"conferenceName":   "",
				"chatId":           "",
				"isConnected":      false,
				"statusText":       "",
			}
			if title, err := s.tox.ConferenceGetTitle(confID); err == nil {
				conf["conferenceName"] = title
				conf["statusText"] = title
			}
			if chatId, err := s.tox.ConferenceGetIdentifier(confID); err == nil {
				conf["chatId"] = chatId
			}
			s.mu.RLock()
			if connected, ok := s.conferenceConnected[confID]; ok {
				conf["isConnected"] = connected
			}
			s.mu.RUnlock()
			conf["memberCount"] = int(s.tox.ConferencePeerCount(confID))
			conferences = append(conferences, conf)
		}
		resp := map[string]interface{}{
			"conferences": conferences,
		}
		json.NewEncoder(w).Encode(resp)
	} else if r.Method == http.MethodPost {
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

	chatId, _ := s.tox.GroupGetChatId(groupNumber)
	chanidInt, _ := s.getOrCreatePubKeyID(chatId)
	selfPubKey := s.tox.SelfGetPublicKey()
	senderInt, _ := s.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	if true {
		s.persistEventToSQLite(chanidInt, string(data))
	}

	resp := map[string]interface{}{
		"message_id": uint64(msgId),
		"message":    "message sent",
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

	chatId, _ := s.tox.ConferenceGetIdentifier(confID)
	chanidInt, _ := s.getOrCreatePubKeyID(chatId)
	selfPubKey := s.tox.SelfGetPublicKey()
	senderInt, _ := s.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	if false {
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

	if name == "" {
		name = s.tox.SelfGetName()
		if name == "" {
			pubkey := s.tox.SelfGetPublicKey()
			if len(pubkey) >= 5 {
				name = "nonamed" + pubkey[:5]
			} else {
				name = "nonamed"
			}
		}
		log.Printf("[GroupJoin] name auto-filled: %s", name)
	}

	if len(chatId) != 64 {
		http.Error(w, fmt.Sprintf(`{"error":"invalid chat_id length: expected 64, got %d"}`, len(chatId)), http.StatusBadRequest)
		return
	}

	if !regexp.MustCompile(`^[0-9a-fA-F]{64}$`).MatchString(chatId) {
		http.Error(w, `{"error":"invalid chat_id format: must be 64 hex characters (0-9, A-F)"}`, http.StatusBadRequest)
		return
	}

	log.Printf("[GroupJoin] Attempting to join group: chat_id=%s..., name=%s, has_password=%v",
		safeTruncate(chatId, 16), name, password != "")

	groupNumber, err := s.tox.GroupJoin(chatId, name, password)
	if err != nil {
		errStr := err.Error()
		log.Printf("[GroupJoin] FAILED: chat_id=%s..., error=%s", safeTruncate(chatId, 16), errStr)
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

	log.Printf("[GroupJoin] SUCCESS: group_number=%d, chat_id=%s...", uint32(groupNumber), safeTruncate(chatId, 16))

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

	var timeoutDuration = 30 * time.Second
	if afterID > 0 && afterID >= s.eventQueue.GetNextID() {
		timeoutDuration = 3 * time.Second
	}
	timeout := time.After(timeoutDuration)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-timeout:
			w.Header().Set("X-Server-Next-Id", strconv.FormatUint(s.eventQueue.GetNextID(), 10))
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		case <-ticker.C:
			events := s.eventQueue.PopAfter(afterID)
			if len(events) > 0 {
				w.Header().Set("X-Server-Next-Id", strconv.FormatUint(s.eventQueue.GetNextID(), 10))
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

	absPath := filepath.Join(s.webRoot, path)
	http.ServeFile(w, r, absPath)
}

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

	peers := s.tox.ConferenceGetPeers(confID)

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

	members := make([]map[string]interface{}, 0)
	for peerNumber := 0; peerNumber < 256; peerNumber++ {
		pn := tox.GroupPeerNumber(peerNumber)
		name, err := s.tox.GroupPeerGetName(groupNumber, pn)
		if err != nil {
			break
		}
		pubKey, _ := s.tox.GroupPeerGetPublicKey(groupNumber, pn)
		connStatus, _ := s.tox.GroupPeerGetConnectionStatus(groupNumber, pn)
		role, _ := s.tox.GroupPeerGetRole(groupNumber, pn)
		ip, ipErr := s.toxp.GroupPeerGetIPAddress(uint32(groupNumber), uint32(pn))
		if ipErr != nil {
			log.Printf("[IP] Group %d Peer %d: error: %v", int(groupNumber), peerNumber, ipErr)
		}

		members = append(members, map[string]interface{}{
			"peerNumber": peerNumber,
			"name":       name,
			"iconUrl":    "",
			"status":     connStatus,
			"statusStr":  groupStatusToStr(connStatus),
			"statusText": "",
			"role":       int(role),
			"roleStr":    roleToStr(int(role)),
			"publicKey":  pubKey,
			"isSelf":     false,
			"lastSeen":   nil,
			"peerIp":     ip,
		})
	}

	selfPeerNumber, selfErr := s.tox.GroupSelfGetPeerId(groupNumber)
	for i := range members {
		if members[i]["peerNumber"].(int) == int(selfPeerNumber) {
			members[i]["isSelf"] = true
		}
	}
	resp := map[string]interface{}{
		"groupNumber":     int(groupNumber),
		"members":         members,
		"selfPeerNumber":  int(selfPeerNumber),
	}
	if selfErr != nil {
		resp["selfPeerNumber"] = 0
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(resp)
}

func (s *Server) groupPeerCount(gn tox.GroupNumber) int {
	for peerNum := 0; peerNum < 256; peerNum++ {
		_, err := s.tox.GroupPeerGetName(gn, tox.GroupPeerNumber(peerNum))
		if err != nil {
			return peerNum
		}
	}
	return 256
}

func (s *Server) handleRandomName(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	name := randomName()
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{"name": name})
}

func (s *Server) handleGroupSetName(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	gnStr := params.Get("group_number")
	name := strings.TrimSpace(params.Get("name"))
	if gnStr == "" || name == "" {
		http.Error(w, `{"error":"missing group_number or name"}`, http.StatusBadRequest)
		return
	}
	var gn tox.GroupNumber
	fmt.Sscanf(gnStr, "%d", &gn)
	if err := s.tox.GroupSelfSetName(gn, name); err != nil {
		http.Error(w, fmt.Sprintf(`{"error":"%s"}`, err), http.StatusBadRequest)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]string{"message": "ok"})
}

func (s *Server) handleTranslate(w http.ResponseWriter, r *http.Request) {
	type translateResponse struct {
		TranslatedText string `json:"translated_text,omitempty"`
		Error          string `json:"error,omitempty"`
		Code           string `json:"code,omitempty"`
	}
	writeErr := func(code, msg string, status int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(status)
		json.NewEncoder(w).Encode(translateResponse{Error: msg, Code: code})
	}
	if r.Method != http.MethodPost {
		writeErr("INVALID_METHOD", "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	var req struct {
		Text string `json:"text"`
		To   string `json:"to"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr("INVALID_JSON", fmt.Sprintf("invalid json: %s", err), http.StatusBadRequest)
		return
	}
	if req.Text == "" || req.To == "" {
		writeErr("MISSING_FIELDS", "text and to required", http.StatusBadRequest)
		return
	}
	results, err := oai.MsetTranFull(req.To, "", req.Text)
	if err != nil {
		writeErr("TRANSLATE_FAILED", err.Error(), http.StatusInternalServerError)
		return
	}
	if len(results) == 0 {
		writeErr("EMPTY_RESULT", "translation returned empty result", http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(translateResponse{TranslatedText: results[0]})
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

func (s *Server) getContactNumber(contactType string, contactPubkey string, senderPubkey string) (uint32, error) {
	switch contactType {
	case "friend":
		num, err := s.tox.FriendByPublicKey(senderPubkey)
		if err != nil {
			return ContactNotFound, nil
		}
		return num, nil

	case "group":
		groupNum, err := s.tox.GroupByChatId(contactPubkey)
		if err != nil {
			return ContactNotFound, nil
		}
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
		chatlist := s.tox.ConferenceGetChatlist()
		for _, confNum := range chatlist {
			identifier, err := s.tox.ConferenceGetIdentifier(confNum)
			if err != nil {
				continue
			}
			if strings.EqualFold(identifier, contactPubkey) {
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

func (s *Server) handleMessageHistory(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error":"method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	chanid := r.URL.Query().Get("chanid")
	contactIDStr := r.URL.Query().Get("contact_id")
	contactType := r.URL.Query().Get("contact_type")

	if contactType == "" {
		http.Error(w, `{"error":"missing contact_type"}`, http.StatusBadRequest)
		return
	}

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

	var chanidInt int64
	err := s.db.QueryRow("SELECT pkid FROM pubkey_ids WHERE pubkey = ?", chanid).Scan(&chanidInt)
	if err != nil {
		http.Error(w, `{"error":"invalid chanid"}`, http.StatusBadRequest)
		return
	}

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

		senderPubkey := ""
		if sender, ok := eventData["sender"].(float64); ok {
			var pk string
			if err := s.db.QueryRow("SELECT pubkey FROM pubkey_ids WHERE pkid = ?", int64(sender)).Scan(&pk); err == nil {
				senderPubkey = pk
			}
		}

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

	for i, j := 0, len(messages)-1; i < j; i, j = i+1, j-1 {
		messages[i], messages[j] = messages[j], messages[i]
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"messages": messages,
	})
}
