package server

import (
	"encoding/json"
	"fmt"
	"net/http"
	"sort"
	"strconv"
	"strings"
	"time"
)

const serverSuffix = ":" + matrixHost

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

type roomEntry struct {
	roomID      string
	chatID      string
	contactType string
	name        string
	memberCount int
	isDM        bool
}

// ── MatrixServer v5 sync methods ──

func (ms *MatrixServer) handleV5Sync(w http.ResponseWriter, r *http.Request) {
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

	ms.mu.Lock()
	_, ok := ms.tokens[token]
	ms.mu.Unlock()
	if !ok {
		writeMatrixUnauthorized(w, "M_UNKNOWN_TOKEN", "Invalid access token")
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

	if req.Timeout <= 0 {
		req.Timeout = 0
	} else if req.Timeout > 60000 {
		req.Timeout = 60000
	}

	resp := ms.v5process(&req)
	writeJSON(w, resp)
}

func (ms *MatrixServer) v5process(req *V5SyncRequest) *V5SyncResponse {
	if req.Pos == "" {
		return ms.v5buildInitial(req)
	}
	return ms.v5buildIncremental(req)
}

func (ms *MatrixServer) v5buildInitial(req *V5SyncRequest) *V5SyncResponse {
	ms.v5mu.Lock()
	_, nextID := ms.midapi.EventsPoll(0)
	ms.v5after = nextID
	ms.v5bump++
	pos := fmt.Sprintf("s%d", ms.v5bump)
	ms.v5mu.Unlock()

	rooms := ms.v5collectRooms(req)
	rmap := make(map[string]RoomResult, len(rooms))
	for _, r := range rooms {
		rmap[r.roomID] = ms.v5roomResult(r, true)
	}

	return &V5SyncResponse{
		Pos:   pos,
		Lists: map[string]ListResult{"all": {Count: len(rooms)}},
		Rooms: rmap,
	}
}

func (ms *MatrixServer) v5buildIncremental(req *V5SyncRequest) *V5SyncResponse {
	ms.v5mu.Lock()
	events, nextID := ms.midapi.EventsPoll(ms.v5after)
	ms.v5after = nextID
	ms.v5bump++
	pos := fmt.Sprintf("s%d", ms.v5bump)
	ms.v5mu.Unlock()

	if len(events) == 0 {
		if req.Timeout > 0 {
			time.Sleep(time.Duration(req.Timeout) * time.Millisecond)
			ms.v5mu.Lock()
			events2, nextID2 := ms.midapi.EventsPoll(ms.v5after)
			if len(events2) == 0 {
				ms.v5mu.Unlock()
				return &V5SyncResponse{Pos: pos}
			}
			ms.v5after = nextID2
			ms.v5mu.Unlock()
			events = events2
		} else {
			return &V5SyncResponse{Pos: pos}
		}
	}

	rooms := ms.v5collectRooms(req)
	rmap := make(map[string]RoomResult, len(rooms))
	for _, r := range rooms {
		rmap[r.roomID] = ms.v5roomResult(r, false)
	}

	return &V5SyncResponse{
		Pos:   pos,
		Lists: map[string]ListResult{"all": {Count: len(rooms)}},
		Rooms: rmap,
	}
}

func (ms *MatrixServer) v5collectRooms(req *V5SyncRequest) []roomEntry {
	ids := ms.midapi.FriendsList()
	strIDs := make([]string, len(ids))
	for i, id := range ids {
		strIDs[i] = strconv.FormatUint(uint64(id), 10)
	}
	friends := ms.midapi.FriendsInfo(strIDs)
	groups := ms.midapi.GroupsList()
	confs := ms.midapi.ConferencesList()

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
			memberCount: 2,
		}
		if f.Name != "" {
			entry.name = f.Name
		}
		if ms.v5filterRoom(entry, req) {
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
		if ms.v5filterRoom(entry, req) {
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
		if ms.v5filterRoom(entry, req) {
			all = append(all, entry)
		}
	}

	ms.v5mu.Lock()
	base := ms.v5bump
	ms.v5mu.Unlock()
	sort.Slice(all, func(i, j int) bool {
		return (base - int64(i)) > (base - int64(j))
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

func (ms *MatrixServer) v5filterRoom(r roomEntry, req *V5SyncRequest) bool {
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

func (ms *MatrixServer) v5roomResult(r roomEntry, initial bool) RoomResult {
	membership := "join"
	var heroes []Hero
	if r.contactType == "friend" {
		heroes = []Hero{{UserID: "@" + r.chatID + serverSuffix}}
	}

	var name *string
	if r.name != "" {
		name = &r.name
	}

	ms.v5mu.Lock()
	ms.v5bump++
	bump := ms.v5bump
	ms.v5mu.Unlock()

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
