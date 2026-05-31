package server

import (
	"encoding/json"
	"fmt"
	"net/http"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

const serverSuffix = ":127.0.0.1"

func roomID(chatID string) string {
	return "!" + chatID + serverSuffix
}

func parseRoomID(rid string) string {
	if !strings.HasPrefix(rid, "!") || !strings.HasSuffix(rid, serverSuffix) {
		return ""
	}
	return rid[1 : len(rid)-len(serverSuffix)]
}

type V5SyncRequest struct {
	ConnID            string                      `json:"conn_id,omitempty"`
	Pos               string                      `json:"pos,omitempty"`
	Timeout           int                         `json:"timeout,omitempty"`
	SetPresence       string                      `json:"set_presence,omitempty"`
	Lists             map[string]SyncListConfig   `json:"lists,omitempty"`
	RoomSubscriptions map[string]RoomSubscription `json:"room_subscriptions,omitempty"`
	Extensions        map[string]json.RawMessage  `json:"extensions,omitempty"`
}

type SyncListConfig struct {
	TimelineLimit int                `json:"timeline_limit"`
	RequiredState RequiredStateReq   `json:"required_state"`
	Range         [2]int             `json:"range,omitempty"`
	Filters       *SlidingRoomFilter `json:"filters,omitempty"`
}

type RoomSubscription struct {
	TimelineLimit int              `json:"timeline_limit"`
	RequiredState RequiredStateReq `json:"required_state"`
}

type RequiredStateReq struct {
	Include     []RequiredStateElement `json:"include,omitempty"`
	Exclude     []RequiredStateElement `json:"exclude,omitempty"`
	LazyMembers bool                   `json:"lazy_members,omitempty"`
}

type RequiredStateElement struct {
	Type     string `json:"type,omitempty"`
	StateKey string `json:"state_key,omitempty"`
}

type SlidingRoomFilter struct {
	IsDM         *bool     `json:"is_dm,omitempty"`
	Spaces       []string  `json:"spaces,omitempty"`
	IsEncrypted  *bool     `json:"is_encrypted,omitempty"`
	IsInvited    *bool     `json:"is_invited,omitempty"`
	RoomTypes    []*string `json:"room_types,omitempty"`
	NotRoomTypes []*string `json:"not_room_types,omitempty"`
	Tags         []string  `json:"tags,omitempty"`
	NotTags      []string  `json:"not_tags,omitempty"`
}

type V5SyncResponse struct {
	Pos        string                 `json:"pos"`
	Lists      map[string]ListResult  `json:"lists,omitempty"`
	Rooms      map[string]RoomResult  `json:"rooms,omitempty"`
	Extensions map[string]interface{} `json:"extensions,omitempty"`
}

type ListResult struct {
	Count int `json:"count"`
}

type RoomResult struct {
	Initial     bool              `json:"initial"`
	BumpStamp   int64             `json:"bump_stamp"`
	Membership  string            `json:"membership"`
	Name        *string           `json:"name,omitempty"`
	IsDM        *bool             `json:"is_dm,omitempty"`
	Heroes      []Hero            `json:"heroes,omitempty"`
	JoinedCount int               `json:"joined_count,omitempty"`
	Timeline    []json.RawMessage `json:"timeline,omitempty"`
	PrevBatch   string            `json:"prev_batch,omitempty"`
	NumLive     int               `json:"num_live,omitempty"`
	Limited     bool              `json:"limited,omitempty"`
}

type Hero struct {
	UserID string `json:"user_id"`
}

type SyncProvider interface {
	SelfGet() *SelfInfo
	AllFriends() []FriendInfo
	AllGroups() []GroupInfo
	AllConferences() []ConferenceInfo
	GroupMembers(gn uint32) *GroupMemberList
	ChanIDForContact(contactID uint32, contactType string) (string, error)
	MessageHistory(chanid, contactType string) ([]MessageRecord, error)
	EventQueue() *EventQueue
}

type roomEntry struct {
	roomID      string
	chatID      string
	contactType string
	name        string
	memberCount int
	isDM        bool
}

type SyncSession struct {
	ConnID string
	Pos    string
	After  uint64
}

type SyncManager struct {
	provider SyncProvider
	mu       sync.Mutex
	sessions map[string]*SyncSession
	bumpSeq  atomic.Int64
}

func NewSyncManager(p SyncProvider) *SyncManager {
	return &SyncManager{
		provider: p,
		sessions: make(map[string]*SyncSession),
	}
}

func (sm *SyncManager) HandleV5Sync(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Only POST is supported",
		})
		return
	}

	token := extractBearer(r)
	if token == "" {
		writeMatrixUnauthorized(w, "M_MISSING_TOKEN", "Missing access token")
		return
	}

	var req V5SyncRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, map[string]string{
			"errcode": "M_BAD_JSON",
			"error":   "Invalid JSON: " + err.Error(),
		})
		return
	}

	if req.ConnID == "" {
		req.ConnID = "default"
	}
	if req.Timeout <= 0 {
		req.Timeout = 0
	} else if req.Timeout > 60000 {
		req.Timeout = 60000
	}

	resp := sm.process(token, &req)
	writeJSON(w, resp)
}

func (sm *SyncManager) process(token string, req *V5SyncRequest) *V5SyncResponse {
	sm.mu.Lock()
	ses, ok := sm.sessions[req.ConnID]
	if !ok {
		ses = &SyncSession{ConnID: req.ConnID}
		sm.sessions[req.ConnID] = ses
	}
	sm.mu.Unlock()

	if req.Pos == "" || req.Pos != ses.Pos {
		return sm.buildInitial(ses, req)
	}
	return sm.buildIncremental(ses, req)
}

func (sm *SyncManager) buildInitial(ses *SyncSession, req *V5SyncRequest) *V5SyncResponse {
	eq := sm.provider.EventQueue()
	_, nextID := eq.PopAfter(0)
	ses.After = nextID

	rooms := sm.collectRooms(req)
	pos := fmt.Sprintf("s%d", sm.bumpSeq.Add(1))
	ses.Pos = pos

	rmap := make(map[string]RoomResult, len(rooms))
	for _, r := range rooms {
		rmap[r.roomID] = sm.roomResult(r, true)
	}

	count := len(rooms)
	if lst, ok := req.Lists["all"]; ok && len(lst.Range) == 2 {
		start, end := lst.Range[0], lst.Range[1]
		if start >= 0 && start <= end {
			if end >= count {
				end = count - 1
			}
			count = end - start + 1
		}
	}

	return &V5SyncResponse{
		Pos:   pos,
		Lists: map[string]ListResult{"all": {Count: len(rooms)}},
		Rooms: rmap,
	}
}

func (sm *SyncManager) buildIncremental(ses *SyncSession, req *V5SyncRequest) *V5SyncResponse {
	eq := sm.provider.EventQueue()
	events, nextID := eq.PopAfter(ses.After)

	pos := fmt.Sprintf("s%d", sm.bumpSeq.Add(1))
	ses.Pos = pos
	ses.After = nextID

	if len(events) == 0 {
		if req.Timeout > 0 {
			time.Sleep(time.Duration(req.Timeout) * time.Millisecond)
			events2, nextID2 := eq.PopAfter(ses.After)
			if len(events2) == 0 {
				ses.Pos = pos
				return &V5SyncResponse{Pos: pos}
			}
			ses.After = nextID2
			events = events2
		} else {
			return &V5SyncResponse{Pos: pos}
		}
	}

	rooms := sm.collectRooms(req)
	rmap := make(map[string]RoomResult, len(rooms))
	for _, r := range rooms {
		rmap[r.roomID] = sm.roomResult(r, false)
	}

	return &V5SyncResponse{
		Pos:   pos,
		Lists: map[string]ListResult{"all": {Count: len(rooms)}},
		Rooms: rmap,
	}
}

func (sm *SyncManager) collectRooms(req *V5SyncRequest) []roomEntry {
	friends := sm.provider.AllFriends()
	groups := sm.provider.AllGroups()
	confs := sm.provider.AllConferences()

	all := make([]roomEntry, 0, len(friends)+len(groups)+len(confs))

	for _, f := range friends {
		chatID := f.PublicKey
		if chatID == "" {
			continue
		}
		entry := roomEntry{
			roomID:      roomID(chatID),
			chatID:      chatID,
			contactType: "friend",
			isDM:        true,
			memberCount: 1,
		}
		if f.Name != "" {
			entry.name = f.Name
		}
		if sm.filterRoom(entry, req) {
			all = append(all, entry)
		}
	}

	for _, g := range groups {
		chatID := g.ChatId
		if chatID == "" {
			continue
		}
		entry := roomEntry{
			roomID:      roomID(chatID),
			chatID:      chatID,
			contactType: "group",
			name:        g.GroupName,
			memberCount: g.MemberCount,
			isDM:        false,
		}
		if sm.filterRoom(entry, req) {
			all = append(all, entry)
		}
	}

	for _, c := range confs {
		chatID := c.ChatId
		if chatID == "" {
			continue
		}
		entry := roomEntry{
			roomID:      roomID(chatID),
			chatID:      chatID,
			contactType: "conference",
			name:        c.ConferenceName,
			memberCount: c.MemberCount,
			isDM:        false,
		}
		if sm.filterRoom(entry, req) {
			all = append(all, entry)
		}
	}

	bumpSeq := sm.bumpSeq.Add(int64(len(all)))
	sort.Slice(all, func(i, j int) bool {
		return (bumpSeq - int64(i)) > (bumpSeq - int64(j))
	})

	if len(req.Lists) == 0 {
		return all
	}

	for _, lst := range req.Lists {
		if len(lst.Range) != 2 {
			continue
		}
		start, end := lst.Range[0], lst.Range[1]
		if start < 0 || start > end || start >= len(all) {
			continue
		}
		if end >= len(all) {
			end = len(all) - 1
		}
		return all[start : end+1]
	}

	return all
}

func (sm *SyncManager) filterRoom(r roomEntry, req *V5SyncRequest) bool {
	if len(req.Lists) == 0 && len(req.RoomSubscriptions) == 0 {
		return true
	}

	if _, subbed := req.RoomSubscriptions[r.roomID]; subbed {
		return true
	}

	for _, lst := range req.Lists {
		if lst.Filters == nil {
			return true
		}
		f := lst.Filters
		if f.IsDM != nil && *f.IsDM != r.isDM {
			continue
		}
		if f.IsInvited != nil {
			continue
		}
		return true
	}
	return false
}

func (sm *SyncManager) roomResult(r roomEntry, initial bool) RoomResult {
	u := r.roomID
	membership := "join"
	if r.contactType == "friend" {
		u = "@" + r.chatID + serverSuffix
	}
	heroes := []Hero{{UserID: u}}

	var name *string
	if r.name != "" {
		name = &r.name
	}

	bump := sm.bumpSeq.Add(1)

	res := RoomResult{
		Initial:     initial,
		BumpStamp:   bump,
		Membership:  membership,
		Name:        name,
		Heroes:      heroes,
		JoinedCount: r.memberCount,
	}
	if r.isDM {
		res.IsDM = &r.isDM
	}

	return res
}
