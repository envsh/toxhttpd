package server

import (
	"fmt"
	"net/http"
	"strconv"
	"time"
)

type v3EventList struct {
	Events []interface{} `json:"events"`
}

type v3DeviceLists struct {
	Changed []string `json:"changed"`
	Left    []string `json:"left"`
}

type v3SyncResponse struct {
	NextBatch              string         `json:"next_batch"`
	Presence               *v3EventList   `json:"presence,omitempty"`
	AccountData            *v3EventList   `json:"account_data,omitempty"`
	ToDevice               *v3EventList   `json:"to_device,omitempty"`
	DeviceLists            *v3DeviceLists `json:"device_lists,omitempty"`
	DeviceOneTimeKeysCount map[string]int `json:"device_one_time_keys_count,omitempty"`
	Rooms                  *v3Rooms       `json:"rooms,omitempty"`
}

type v3Rooms struct {
	Join   map[string]*v3Room     `json:"join,omitempty"`
	Invite map[string]interface{} `json:"invite,omitempty"`
	Leave  map[string]interface{} `json:"leave,omitempty"`
}

type v3Room struct {
	Timeline            *v3Timeline    `json:"timeline,omitempty"`
	State               *v3State       `json:"state,omitempty"`
	AccountData         *v3AccountData `json:"account_data,omitempty"`
	Ephemeral           *v3EventList   `json:"ephemeral,omitempty"`
	Summary             v3Summary      `json:"summary"`
	UnreadNotifications v3Unread       `json:"unread_notifications"`
}

type v3Timeline struct {
	Events    []map[string]interface{} `json:"events"`
	Limited   bool                     `json:"limited"`
	PrevBatch string                   `json:"prev_batch"`
}

type v3State struct {
	Events []map[string]interface{} `json:"events"`
}

type v3AccountData struct {
	Events []interface{} `json:"events"`
}

type v3Summary struct {
	Heroes             []string `json:"heroes,omitempty"`
	JoinedMemberCount  int      `json:"joined_member_count"`
	InvitedMemberCount int      `json:"invited_member_count"`
}

type v3Unread struct {
	NotificationCount int `json:"notification_count"`
	HighlightCount    int `json:"highlight_count"`
}

// handleV3Sync 模拟实现 v3 sync。返回伪造数据让 Matrix 客户端（Cinny）正常显示房间列表。
// 初始 sync 立即返回含 state 事件的完整房间数据；增量 sync 短时等待后返回房间元数据（无 state）。
func (ms *MatrixServer) handleV3Sync(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	since := q.Get("since")
	timeout := 30000
	if t := q.Get("timeout"); t != "" {
		if v, err := strconv.Atoi(t); err == nil && v > 0 {
			timeout = v
		}
	}

	resp := &v3SyncResponse{
		NextBatch:              fmt.Sprintf("s%d", time.Now().UnixNano()),
		Presence:               &v3EventList{Events: []interface{}{}},
		AccountData:            &v3EventList{Events: []interface{}{}},
		ToDevice:               &v3EventList{Events: []interface{}{}},
		DeviceLists:            &v3DeviceLists{Changed: []string{}, Left: []string{}},
		DeviceOneTimeKeysCount: map[string]int{"signed_curve25519": 50},
	}

	if since == "" {
		entries := ms.v3collectRooms()
		join := make(map[string]*v3Room, len(entries))
		for _, r := range entries {
			join[r.roomID] = ms.v3BuildRoom(r)
		}
		resp.Rooms = &v3Rooms{
			Join:   join,
			Invite: map[string]interface{}{},
			Leave:  map[string]interface{}{},
		}
	} else {
		if timeout > 5000 {
			timeout = 5000
		}
		time.Sleep(time.Duration(timeout) * time.Millisecond)
		entries := ms.v3collectRooms()
		join := make(map[string]*v3Room, len(entries))
		for _, r := range entries {
			join[r.roomID] = &v3Room{
				Summary:             ms.v3BuildSummary(r),
				Ephemeral:           &v3EventList{Events: []interface{}{}},
				UnreadNotifications: v3Unread{},
			}
		}
		resp.Rooms = &v3Rooms{Join: join, Invite: map[string]interface{}{}, Leave: map[string]interface{}{}}
	}
	writeJSON(w, resp)
}

func (ms *MatrixServer) v3collectRooms() []roomEntry {
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
		all = append(all, entry)
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
		all = append(all, entry)
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
		all = append(all, entry)
	}

	return all
}

func (ms *MatrixServer) v3BuildSummary(r roomEntry) v3Summary {
	var heroes []string
	if r.contactType == "friend" {
		heroes = []string{"@" + r.chatID + ":" + matrixHost}
	}
	return v3Summary{
		Heroes:             heroes,
		JoinedMemberCount:  r.memberCount,
		InvitedMemberCount: 0,
	}
}

func (ms *MatrixServer) v3BuildRoom(r roomEntry) *v3Room {
	selfID := ms.userID()
	ts := time.Now().UnixMilli()
	tl := &v3Timeline{
		Events:    []map[string]interface{}{},
		Limited:   false,
		PrevBatch: "",
	}
	st := &v3State{
		Events: []map[string]interface{}{
			{
				"type":      "m.room.create",
				"state_key": "",
				"content": map[string]interface{}{
					"creator":      selfID,
					"room_version": "10",
				},
				"event_id":         "$" + r.chatID + "_create:" + matrixHost,
				"room_id":          r.roomID,
				"sender":           selfID,
				"origin_server_ts": ts,
			},
			{
				"type":      "m.room.member",
				"state_key": selfID,
				"content":   map[string]string{"membership": "join"},
				"event_id":         "$" + r.chatID + "_member:" + matrixHost,
				"room_id":          r.roomID,
				"sender":           selfID,
				"origin_server_ts": ts,
			},
		},
	}
	if r.contactType == "friend" {
		friendMXID := "@" + r.chatID + ":" + matrixHost
		st.Events = append(st.Events, map[string]interface{}{
			"type":             "m.room.member",
			"state_key":        friendMXID,
			"content":          map[string]string{"membership": "join"},
			"event_id":         "$" + r.chatID + "_peer:" + matrixHost,
			"room_id":          r.roomID,
			"sender":           friendMXID,
			"origin_server_ts": ts,
		})
	}
	if r.name != "" {
		st.Events = append(st.Events, map[string]interface{}{
			"type":      "m.room.name",
			"state_key": "",
			"content":   map[string]string{"name": r.name},
			"event_id":         "$" + r.chatID + "_name:" + matrixHost,
			"room_id":          r.roomID,
			"sender":           selfID,
			"origin_server_ts": ts,
		})
	}
	st.Events = append(st.Events, map[string]interface{}{
		"type":      "m.room.power_levels",
		"state_key": "",
		"content": map[string]interface{}{
			"ban":    50,
			"events": map[string]interface{}{},
			"events_default":   0,
			"invite":           0,
			"kick":             50,
			"notifications":    map[string]interface{}{"room": 50},
			"redact":           50,
			"state_default":    50,
			"users":            map[string]interface{}{selfID: 100},
			"users_default":    0,
		},
		"event_id":         "$" + r.chatID + "_power_levels:" + matrixHost,
		"room_id":          r.roomID,
		"sender":           selfID,
		"origin_server_ts": ts,
	})
	ad := &v3AccountData{Events: []interface{}{}}
	return &v3Room{
		Timeline:            tl,
		State:               st,
		AccountData:         ad,
		Ephemeral:           &v3EventList{Events: []interface{}{}},
		Summary:             ms.v3BuildSummary(r),
		UnreadNotifications: v3Unread{NotificationCount: 0, HighlightCount: 0},
	}
}
