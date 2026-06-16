package server

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"
)

type Restapi struct {
	m       *Midapi
	webRoot string
}

func NewRestapi(m *Midapi, webRoot string) *Restapi {
	return &Restapi{m: m, webRoot: webRoot}
}

func (h *Restapi) Register(mux *http.ServeMux) {
	mux.HandleFunc("/api/self", corsMiddleware(loggingMiddleware(h.handleSelf)))
	mux.HandleFunc("/api/friends", corsMiddleware(loggingMiddleware(h.handleFriends)))
	mux.HandleFunc("/api/friend_delete", corsMiddleware(loggingMiddleware(h.handleFriendDelete)))
	mux.HandleFunc("/api/friend", corsMiddleware(loggingMiddleware(h.handleFriendInfo)))
	mux.HandleFunc("/api/messages", corsMiddleware(loggingMiddleware(h.handleSendMessage)))
	mux.HandleFunc("/api/groups", corsMiddleware(loggingMiddleware(h.handleGroups)))
	mux.HandleFunc("/api/conferences", corsMiddleware(loggingMiddleware(h.handleConferences)))
	mux.HandleFunc("/api/conference_messages", corsMiddleware(loggingMiddleware(h.handleConferenceMessages)))
	mux.HandleFunc("/api/conferences/join", corsMiddleware(loggingMiddleware(h.handleConferenceJoin)))
	mux.HandleFunc("/api/conferences/reject", corsMiddleware(loggingMiddleware(h.handleConferenceReject)))
	mux.HandleFunc("/api/conferences/ignore", corsMiddleware(loggingMiddleware(h.handleConferenceIgnore)))
	mux.HandleFunc("/api/bootstrap", corsMiddleware(loggingMiddleware(h.handleBootstrap)))
	mux.HandleFunc("/api/events", corsMiddleware(loggingMiddleware(h.handleEvents)))
	mux.HandleFunc("/api/ndevents", corsMiddleware(loggingMiddleware(h.handleNDEvents)))
	mux.HandleFunc("/api/messages/send", corsMiddleware(loggingMiddleware(h.handleMessageSend)))
	mux.HandleFunc("/api/messages/history", corsMiddleware(loggingMiddleware(h.handleMessageHistory)))
	mux.HandleFunc("/", corsMiddleware(loggingMiddleware(h.handleWeb)))
	mux.HandleFunc("/api/groups/join", corsMiddleware(loggingMiddleware(h.handleGroupJoin)))
	mux.HandleFunc("/api/groups/leave", corsMiddleware(loggingMiddleware(h.handleGroupLeave)))
	mux.HandleFunc("/api/group_messages", corsMiddleware(loggingMiddleware(h.handleGroupSendMessage)))
	mux.HandleFunc("/api/groups/invite", corsMiddleware(loggingMiddleware(h.handleGroupInvite)))
	mux.HandleFunc("/api/groups/accept", corsMiddleware(loggingMiddleware(h.handleGroupAccept)))
	mux.HandleFunc("/api/conference/members", corsMiddleware(loggingMiddleware(h.handleConferenceMembers)))
	mux.HandleFunc("/api/group/members", corsMiddleware(loggingMiddleware(h.handleGroupMembers)))
	mux.HandleFunc("/api/random-name", corsMiddleware(loggingMiddleware(h.handleRandomName)))
	mux.HandleFunc("/api/groups/set-self-name", corsMiddleware(loggingMiddleware(h.handleGroupSetName)))
	mux.HandleFunc("/api/groups/set-topic", corsMiddleware(loggingMiddleware(h.handleGroupSetTopic)))
	mux.HandleFunc("/api/conferences/set-title", corsMiddleware(loggingMiddleware(h.handleConferenceSetTitle)))
	mux.HandleFunc("/api/conferences/leave", corsMiddleware(loggingMiddleware(h.handleConferenceLeave)))
	mux.HandleFunc("/api/toxiterate", corsMiddleware(loggingMiddleware(h.handleToxIterate)))
	mux.HandleFunc("/api/translate", corsMiddleware(loggingMiddleware(h.handleTranslate)))
}

func writeJSON(w http.ResponseWriter, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(v)
}

func WriteJsonErr(w http.ResponseWriter, msg string, code int) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(map[string]string{"error": msg})
}

func writeErr(w http.ResponseWriter, msg string, code int) {
	WriteJsonErr(w, msg, code)
}

// ── Self ──

func (h *Restapi) handleSelf(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodPost {
		params, err := getRequestParams(r)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		info, err := h.m.SelfUpdate(params.Get("name"), params.Get("status_message"))
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{
			"message":           "updated",
			"address":           info.Address,
			"name":              info.Name,
			"status_message":    info.StatusMessage,
			"connection_status": info.ConnectionStatus,
		})
		return
	}

	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	writeJSON(w, h.m.SelfGet())
}

// ── Friends ──

func (h *Restapi) handleFriends(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		friends := h.m.FriendsList()
		json.NewEncoder(w).Encode(map[string]interface{}{"friends": friends})

	} else if r.Method == http.MethodPost {
		params, err := getRequestParams(r)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		pubkey := params.Get("public_key")
		if pubkey == "" {
			writeErr(w, "missing public_key", http.StatusBadRequest)
			return
		}
		fn, err := h.m.FriendAdd(pubkey, params.Get("message"))
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{
			"friend_id": fn,
			"message":   "friend request sent",
		})

	} else if r.Method == http.MethodDelete {
		params, err := getRequestParams(r)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		var friendID uint32
		fmt.Sscanf(params.Get("friend_id"), "%d", &friendID)
		if err := h.m.FriendDelete(friendID); err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]string{"message": "friend deleted"})
	}
}

func (h *Restapi) handleFriendInfo(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	idsStr := params.Get("friend_ids")
	idStrs := strings.Split(idsStr, ",")
	writeJSON(w, h.m.FriendsInfo(idStrs))
}

// ── Messages ──

func (h *Restapi) handleSendMessage(w http.ResponseWriter, r *http.Request) {
	log.Printf("[DEPRECATED] POST /api/messages is deprecated, use POST /api/messages/send instead")
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	var friendID uint32
	fmt.Sscanf(params.Get("friend_id"), "%d", &friendID)
	msgID, err := h.m.SendFriendMessage(friendID, params.Get("message"))
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{"message_id": msgID})
}

func (h *Restapi) handleGroupSendMessage(w http.ResponseWriter, r *http.Request) {
	log.Printf("[DEPRECATED] POST /api/group_messages is deprecated, use POST /api/messages/send instead")
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	gnStr := params.Get("group_number")
	message := params.Get("message")
	if gnStr == "" || message == "" {
		writeErr(w, "missing required parameters", http.StatusBadRequest)
		return
	}
	var gn uint32
	fmt.Sscanf(gnStr, "%d", &gn)
	msgId, err := h.m.SendGroupMessage(gn, params.Get("message_type"), message)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{
		"message_id": msgId,
		"message":    "message sent",
	})
}

func (h *Restapi) handleConferenceMessages(w http.ResponseWriter, r *http.Request) {
	log.Printf("[DEPRECATED] POST /api/conference_messages is deprecated, use POST /api/messages/send instead")
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	var confID uint32
	fmt.Sscanf(params.Get("conference_id"), "%d", &confID)
	if err := h.m.SendConferenceMessage(confID, params.Get("message")); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{
		"conference_id": confID,
		"message":       "sent",
	})
}

func (h *Restapi) handleMessageSend(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}

	chatType := params.Get("type")
	idStr := params.Get("id")
	message := params.Get("message")

	if chatType == "" || idStr == "" || message == "" {
		writeErr(w, "missing required parameters: type, id, message", http.StatusBadRequest)
		return
	}

	var id uint32
	fmt.Sscanf(idStr, "%d", &id)

	switch chatType {
	case "friend":
		msgID, err := h.m.SendFriendMessage(id, message)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{"message_id": msgID})

	case "conference":
		err := h.m.SendConferenceMessage(id, message)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{"message": "sent"})

	case "group":
		messageType := params.Get("message_type")
		msgId, err := h.m.SendGroupMessage(id, messageType, message)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{"message_id": msgId})

	default:
		writeErr(w, "unknown type: "+chatType, http.StatusBadRequest)
	}
}

// ── Groups ──

func (h *Restapi) handleGroups(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		json.NewEncoder(w).Encode(map[string]interface{}{
			"groups": h.m.GroupsList(),
		})
	} else if r.Method == http.MethodPost {
		params, err := getRequestParams(r)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		gn, err := h.m.GroupCreate(
			params.Get("privacy_state"),
			params.Get("group_name"),
			params.Get("name"),
			params.Get("password"),
		)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{
			"group_number": gn,
			"message":      "group created",
		})
	}
}

func (h *Restapi) handleGroupJoin(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}

	chatId := params.Get("chat_id")
	if chatId == "" {
		writeErr(w, "missing chat_id", http.StatusBadRequest)
		return
	}
	if len(chatId) != 64 {
		writeErr(w, fmt.Sprintf("invalid chat_id length: expected 64, got %d", len(chatId)), http.StatusBadRequest)
		return
	}
	if !regexp.MustCompile(`^[0-9a-fA-F]{64}$`).MatchString(chatId) {
		writeErr(w, "invalid chat_id format: must be 64 hex characters (0-9, A-F)", http.StatusBadRequest)
		return
	}

	gn, err := h.m.GroupJoin(chatId, params.Get("name"), params.Get("password"))
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{
		"group_number": gn,
		"message":      "group joined",
	})
}

func (h *Restapi) handleGroupLeave(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	gnStr := params.Get("group_number")
	if gnStr == "" {
		writeErr(w, "missing group_number", http.StatusBadRequest)
		return
	}
	var gn uint32
	fmt.Sscanf(gnStr, "%d", &gn)
	if err := h.m.GroupLeave(gn, params.Get("part_message")); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "group left"})
}

func (h *Restapi) handleGroupInvite(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	gnStr := params.Get("group_number")
	fnStr := params.Get("friend_number")
	if gnStr == "" || fnStr == "" {
		writeErr(w, "missing required parameters", http.StatusBadRequest)
		return
	}
	var gn uint32
	var fn uint32
	fmt.Sscanf(gnStr, "%d", &gn)
	fmt.Sscanf(fnStr, "%d", &fn)
	if err := h.m.GroupInvite(gn, fn); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "invite sent"})
}

func (h *Restapi) handleGroupAccept(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	inviteData := params.Get("invite_data")
	fnStr := params.Get("friend_number")
	if inviteData == "" || fnStr == "" {
		writeErr(w, "missing required parameters", http.StatusBadRequest)
		return
	}
	var fn uint32
	fmt.Sscanf(fnStr, "%d", &fn)
	gn, err := h.m.GroupAccept(inviteData, fn, params.Get("name"), params.Get("password"))
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{
		"group_number": gn,
		"message":      "invite accepted",
	})
}

func (h *Restapi) handleGroupSetName(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	gnStr := params.Get("group_number")
	name := strings.TrimSpace(params.Get("name"))
	if gnStr == "" || name == "" {
		writeErr(w, "missing group_number or name", http.StatusBadRequest)
		return
	}
	var gn uint32
	fmt.Sscanf(gnStr, "%d", &gn)
	if err := h.m.GroupSetName(gn, name); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "ok"})
}

func (h *Restapi) handleGroupSetTopic(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	gnStr := params.Get("group_number")
	topic := strings.TrimSpace(params.Get("topic"))
	if gnStr == "" || topic == "" {
		writeErr(w, "missing group_number or topic", http.StatusBadRequest)
		return
	}
	var gn uint32
	fmt.Sscanf(gnStr, "%d", &gn)
	if err := h.m.GroupSetTopic(gn, topic); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "ok"})
}

// ── Conferences ──

func (h *Restapi) handleConferences(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")

	if r.Method == http.MethodGet {
		json.NewEncoder(w).Encode(map[string]interface{}{
			"conferences": h.m.ConferencesList(),
		})
	} else if r.Method == http.MethodPost {
		confID, err := h.m.ConferenceCreate()
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
		writeJSON(w, map[string]interface{}{
			"conference_id": confID,
			"message":       "Conference created successfully",
		})
	}
}

func (h *Restapi) handleConferenceJoin(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	var fn uint32
	fmt.Sscanf(params.Get("friend_number"), "%d", &fn)
	cookie := params.Get("cookie")
	if cookie == "" {
		writeErr(w, "missing cookie", http.StatusBadRequest)
		return
	}
	confID, err := h.m.ConferenceJoin(fn, cookie)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{
		"conference_id": confID,
		"message":       "Successfully joined conference",
	})
}

func (h *Restapi) handleConferenceReject(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	log.Printf("[TOX] Rejected conference invite from friend %s", params.Get("friend_number"))
	writeJSON(w, map[string]string{"message": "Conference invite rejected"})
}

func (h *Restapi) handleConferenceIgnore(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	log.Printf("[TOX] Ignored conference invite (no action taken)")
	writeJSON(w, map[string]string{"message": "Conference invite ignored"})
}

func (h *Restapi) handleConferenceMembers(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	confIDStr := r.URL.Query().Get("conference_id")
	if confIDStr == "" {
		writeErr(w, "missing conference_id", http.StatusBadRequest)
		return
	}
	var confID uint32
	fmt.Sscanf(confIDStr, "%d", &confID)
	writeJSON(w, h.m.ConferenceMembers(confID))
}

func (h *Restapi) handleConferenceSetTitle(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	confStr := params.Get("conference_id")
	title := strings.TrimSpace(params.Get("title"))
	if confStr == "" || title == "" {
		writeErr(w, "missing conference_id or title", http.StatusBadRequest)
		return
	}
	var confID uint32
	fmt.Sscanf(confStr, "%d", &confID)
	if err := h.m.ConferenceSetTitle(confID, title); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "ok"})
}

func (h *Restapi) handleConferenceLeave(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	confStr := params.Get("conference_id")
	if confStr == "" {
		writeErr(w, "missing conference_id", http.StatusBadRequest)
		return
	}
	var confID uint32
	fmt.Sscanf(confStr, "%d", &confID)
	if err := h.m.ConferenceLeave(confID); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "conference left"})
}

// ── Events ──

func (h *Restapi) handleEvents(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodDelete {
		eventIDStr := r.URL.Query().Get("id")
		var eventID uint64
		fmt.Sscanf(eventIDStr, "%d", &eventID)
		if eventID > 0 {
			h.m.EventsDelete(eventID)
			writeJSON(w, map[string]string{"message": "event deleted"})
			return
		}
		writeErr(w, "invalid event id", http.StatusBadRequest)
		return
	}

	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	if strings.Contains(r.Header.Get("Accept"), "application/x-ndjson") {
		h.handleNDEvents(w, r)
		return
	}

	h.handleHugeEvents(w, r)
}

func (h *Restapi) handleHugeEvents(w http.ResponseWriter, r *http.Request) {
	after := r.URL.Query().Get("after")
	var afterID uint64
	fmt.Sscanf(after, "%d", &afterID)

	// afterID from web/qltox = last seen event ID; add 1 to convert to
	// "next expected ID" for PopAfter (which uses >=). Matrix stores
	// nextID directly and calls PopAfter via Midapi.EventsPoll — unaffected.
	if afterID > 0 {
		afterID++
	}

	if afterID > 0 {
		events, nextID := h.m.EventsPoll(afterID)
		if len(events) > 0 {
			w.Header().Set("X-Server-Next-Id", strconv.FormatUint(nextID, 10))
			w.Header().Set("Content-Type", "application/json")
			json.NewEncoder(w).Encode(events)
			return
		}
	}
	timeout := time.After(30 * time.Second)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-timeout:
			_, nextID := h.m.EventsPoll(afterID)
			w.Header().Set("X-Server-Next-Id", strconv.FormatUint(nextID, 10))
			w.Header().Set("Content-Type", "application/json")
			w.Write([]byte("[]"))
			return
		case <-ticker.C:
			events, nextID := h.m.EventsPoll(afterID)
			if len(events) > 0 {
				w.Header().Set("X-Server-Next-Id", strconv.FormatUint(nextID, 10))
				w.Header().Set("Content-Type", "application/json")
				json.NewEncoder(w).Encode(events)
				return
			}
		}
	}
}

// ── NDJSON Events (newline-delimited JSON, same long-poll logic) ──

func (h *Restapi) handleNDEvents(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	after := r.URL.Query().Get("after")
	var afterID uint64
	fmt.Sscanf(after, "%d", &afterID)
	if afterID > 0 {
		afterID++
	}

	if afterID > 0 {
		events, nextID := h.m.EventsPoll(afterID)
		if len(events) > 0 {
			w.Header().Set("X-Server-Next-Id", strconv.FormatUint(nextID, 10))
			w.Header().Set("Content-Type", "application/x-ndjson")
			for _, e := range events {
				b, _ := json.Marshal(e)
				w.Write(b)
				w.Write([]byte("\n"))
			}
			return
		}
	}

	timeout := time.After(30 * time.Second)
	ticker := time.NewTicker(100 * time.Millisecond)
	defer ticker.Stop()

	w.Header().Set("Content-Type", "application/x-ndjson")
	for {
		select {
		case <-timeout:
			_, nextID := h.m.EventsPoll(afterID)
			w.Header().Set("X-Server-Next-Id", strconv.FormatUint(nextID, 10))
			return
		case <-ticker.C:
			events, nextID := h.m.EventsPoll(afterID)
			if len(events) > 0 {
				w.Header().Set("X-Server-Next-Id", strconv.FormatUint(nextID, 10))
				for _, e := range events {
					b, _ := json.Marshal(e)
					w.Write(b)
					w.Write([]byte("\n"))
				}
				return
			}
		}
	}
}

// ── Bootstrap ──

func (h *Restapi) handleBootstrap(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	log.Println("Bootstrap requested")
	h.m.BootstrapAll()
	writeJSON(w, map[string]string{"message": "bootstrap initiated to 3 nodes"})
}

// ── Random name ──

func (h *Restapi) handleRandomName(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	writeJSON(w, map[string]string{"name": h.m.RandomName()})
}

// ── Group members ──

func (h *Restapi) handleGroupMembers(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	gnStr := r.URL.Query().Get("group_number")
	if gnStr == "" {
		writeErr(w, "missing group_number", http.StatusBadRequest)
		return
	}
	var gn uint32
	fmt.Sscanf(gnStr, "%d", &gn)
	writeJSON(w, h.m.GroupMembers(gn))
}

// ── Tox Iterate ──

func (h *Restapi) handleToxIterate(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	timeoutStr := params.Get("timeout")
	if timeoutStr == "" {
		writeErr(w, "missing timeout", http.StatusBadRequest)
		return
	}
	var timeout int
	fmt.Sscanf(timeoutStr, "%d", &timeout)
	if timeout < 3 {
		timeout = 3
	}
	if timeout > 60 {
		timeout = 60
	}
	start := time.Now()
	count := h.m.ToxIterate(time.Duration(timeout) * time.Second)
	elapsed := time.Since(start).Milliseconds()
	writeJSON(w, map[string]interface{}{
		"iterations": count,
		"elapsed_ms": elapsed,
		"timeout":    timeout,
	})
}

// ── Translate ──

func (h *Restapi) handleTranslate(w http.ResponseWriter, r *http.Request) {
	type translateResponse struct {
		TranslatedText string `json:"translated_text,omitempty"`
		Error          string `json:"error,omitempty"`
		Code           string `json:"code,omitempty"`
	}
	writeTranslationErr := func(code, msg string, status int) {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(status)
		json.NewEncoder(w).Encode(translateResponse{Error: msg, Code: code})
	}
	if r.Method != http.MethodPost {
		writeTranslationErr("INVALID_METHOD", "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	var req struct {
		Text string `json:"text"`
		To   string `json:"to"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeTranslationErr("INVALID_JSON", fmt.Sprintf("invalid json: %s", err), http.StatusBadRequest)
		return
	}
	if req.Text == "" || req.To == "" {
		writeTranslationErr("MISSING_FIELDS", "text and to required", http.StatusBadRequest)
		return
	}
	result, err := h.m.Translate(req.Text, req.To)
	if err != nil {
		writeTranslationErr("TRANSLATE_FAILED", err.Error(), http.StatusInternalServerError)
		return
	}
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(translateResponse{TranslatedText: result})
}

// ── Friend delete ──

func (h *Restapi) handleFriendDelete(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete && r.Method != http.MethodPost {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}
	params, err := getRequestParams(r)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	var friendID uint32
	fmt.Sscanf(params.Get("friend_id"), "%d", &friendID)
	if err := h.m.FriendDelete(friendID); err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]string{"message": "friend deleted"})
}

// ── Message history ──

func (h *Restapi) handleMessageHistory(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeErr(w, "method not allowed", http.StatusMethodNotAllowed)
		return
	}

	chanid := r.URL.Query().Get("chanid")
	contactIDStr := r.URL.Query().Get("contact_id")
	contactType := r.URL.Query().Get("contact_type")

	if contactIDStr != "" {
		id, err := strconv.ParseUint(contactIDStr, 10, 32)
		if err != nil {
			writeErr(w, "invalid contact_id", http.StatusBadRequest)
			return
		}
		chanid, err = h.m.ChanIDForContact(uint32(id), contactType)
		if err != nil {
			writeErr(w, err.Error(), http.StatusBadRequest)
			return
		}
	}

	messages, err := h.m.MessageHistory(chanid, contactType)
	if err != nil {
		writeErr(w, err.Error(), http.StatusBadRequest)
		return
	}
	writeJSON(w, map[string]interface{}{"messages": messages})
}

// ── Web (static files) ──

func (h *Restapi) handleWeb(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Path
	if path == "/" {
		path = "/web/index.html"
	}
	absPath := filepath.Join(h.webRoot, path)
	if _, err := os.Stat(absPath); os.IsNotExist(err) {
		WriteJsonErr(w, "not found", http.StatusNotFound)
		return
	}
	http.ServeFile(w, r, absPath)
}
