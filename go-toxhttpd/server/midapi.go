package server

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"strings"
	"time"

	tox "github.com/TokTok/go-toxcore-c"
	"github.com/kitech/touse/oai"
)

// ── Result types ──

type SelfInfo struct {
	Address          string `json:"address"`
	Name             string `json:"name"`
	StatusMessage    string `json:"status_message"`
	StatusEmoji      string `json:"status_emoji"`
	ConnectionStatus string `json:"connection_status"`
}

type FriendInfo struct {
	FriendID    uint32      `json:"friendId"`
	Name        string      `json:"name"`
	IconUrl     string      `json:"iconUrl"`
	Status      int         `json:"status"`
	StatusStr   string      `json:"statusStr"`
	UserStatus  int         `json:"userStatus"`
	StatusText  string      `json:"statusText"`
	PublicKey   string      `json:"publicKey"`
	LastSeen    uint64      `json:"lastSeen"`
	PeerIp      interface{} `json:"peerIp"`
	Error       string      `json:"error,omitempty"`
}

type GroupInfo struct {
	GroupNumber  int    `json:"groupNumber"`
	GroupName    string `json:"groupName"`
	ChatId       string `json:"chatId"`
	IsConnected  bool   `json:"isConnected"`
	StatusText   string `json:"statusText"`
	MemberCount  int    `json:"memberCount"`
}

type ConferenceInfo struct {
	ConferenceNumber uint32 `json:"conferenceNumber"`
	ConferenceName   string `json:"conferenceName"`
	ChatId           string `json:"chatId"`
	IsConnected      bool   `json:"isConnected"`
	StatusText       string `json:"statusText"`
	MemberCount      int    `json:"memberCount"`
}

type GroupMember struct {
	PeerNumber int         `json:"peerNumber"`
	Name       string      `json:"name"`
	IconUrl    string      `json:"iconUrl"`
	Status     int         `json:"status"`
	StatusStr  string      `json:"statusStr"`
	StatusText string      `json:"statusText"`
	Role       int         `json:"role"`
	RoleStr    string      `json:"roleStr"`
	PublicKey  string      `json:"publicKey"`
	IsSelf     bool        `json:"isSelf"`
	LastSeen   interface{} `json:"lastSeen"`
	PeerIp     string      `json:"peerIp"`
}

type GroupMemberList struct {
	GroupNumber    int            `json:"groupNumber"`
	Members        []GroupMember  `json:"members"`
	SelfPeerNumber int            `json:"selfPeerNumber"`
}

type ConferenceMember struct {
	PeerNumber int    `json:"peer_number"`
	Name       string `json:"name"`
}

type ConferenceMemberList struct {
	ConferenceID int                 `json:"conference_id"`
	Members      []ConferenceMember  `json:"members"`
}

type MessageRecord struct {
	RowID       int64       `json:"rowid"`
	Message     interface{} `json:"message"`
	SenderPubkey string     `json:"sender_pubkey"`
	SenderNumber uint32     `json:"sender_number"`
	Direction   interface{} `json:"direction"`
	CreatedAt   string      `json:"created_at"`
}

const ContactNotFound = uint32(0xFFFFFFFF)

// ── Midapi ──

type Midapi struct {
	ctx *ApiContext
}

func NewMidapi(ctx *ApiContext) *Midapi {
	return &Midapi{ctx: ctx}
}

func (m *Midapi) requestSave() {
	select {
	case m.ctx.SaveRequestCh <- struct{}{}:
	default:
	}
}

// ── Self ──

func (m *Midapi) SelfGet() *SelfInfo {
	addr := m.ctx.Tox.SelfGetAddress()
	name := m.ctx.Tox.SelfGetName()
	status, _ := m.ctx.Tox.SelfGetStatusMessage()

	connStatus := statusToStr(m.ctx.Tox.SelfGetConnectionStatus())

	m.ctx.Mu.Lock()
	m.ctx.SelfConnectionStatus = connStatus
	m.ctx.Mu.Unlock()

	return &SelfInfo{
		Address:          addr,
		Name:             name,
		StatusMessage:    status,
		StatusEmoji:      "",
		ConnectionStatus: connStatus,
	}
}

func (m *Midapi) SelfUpdate(name, statusMessage string) (*SelfInfo, error) {
	if name != "" {
		if err := m.ctx.Tox.SelfSetName(name); err != nil {
			return nil, fmt.Errorf("failed to set name: %s", err)
		}
		log.Printf("[TOX] Self name updated: %s", name)
	}

	if statusMessage != "" {
		if _, err := m.ctx.Tox.SelfSetStatusMessage(statusMessage); err != nil {
			return nil, fmt.Errorf("failed to set status: %s", err)
		}
		log.Printf("[TOX] Self status message updated: %s", statusMessage)
	}

	m.requestSave()
	return m.SelfGet(), nil
}

// ── Friends ──

func (m *Midapi) FriendsList() []uint32 {
	return m.ctx.Tox.SelfGetFriendList()
}

func (m *Midapi) FriendAdd(pubkey, message string) (uint32, error) {
	if message == "" {
		message = "Hello, I'm using toxhttpd-go!"
	}
	fn, err := m.ctx.Tox.FriendAdd(pubkey, message)
	if err != nil {
		return 0, err
	}
	m.requestSave()
	return fn, nil
}

func (m *Midapi) FriendDelete(friendID uint32) error {
	_, err := m.ctx.Tox.FriendDelete(friendID)
	if err != nil {
		return err
	}
	m.requestSave()
	return nil
}

func (m *Midapi) FriendsInfo(ids []string) []FriendInfo {
	result := make([]FriendInfo, 0, len(ids))
	for _, idStr := range ids {
		idStr = strings.TrimSpace(idStr)
		fi := FriendInfo{
			FriendID: 0, Name: "", IconUrl: "",
			Status: 0, StatusStr: "none",
			UserStatus: 0, StatusText: "", PublicKey: "",
			LastSeen: 0, PeerIp: nil, Error: "",
		}

		var fid uint32
		if _, err := fmt.Sscanf(idStr, "%d", &fid); err != nil {
			fi.Error = "invalid friend id"
			result = append(result, fi)
			continue
		}
		fi.FriendID = fid

		name, err := m.ctx.Tox.FriendGetName(fid)
		if err != nil {
			fi.Error = err.Error()
			result = append(result, fi)
			continue
		}

		pk, _ := m.ctx.Tox.FriendGetPublicKey(fid)
		statusText, _ := m.ctx.Tox.FriendGetStatusMessage(fid)
		lastSeen, _ := m.ctx.Tox.FriendGetLastOnline(fid)

		m.ctx.Mu.RLock()
		connStatus := m.ctx.FriendStatuses[fid]
		m.ctx.Mu.RUnlock()

		statusInt := 0
		switch connStatus {
		case "tcp":
			statusInt = 1
		case "udp":
			statusInt = 2
		}

		m.ctx.Mu.RLock()
		userStatus := m.ctx.FriendUserStatuses[fid]
		m.ctx.Mu.RUnlock()

		fi.Name = name
		fi.Status = statusInt
		fi.StatusStr = statusToStr(statusInt)
		fi.UserStatus = userStatus
		fi.StatusText = statusText
		fi.PublicKey = pk
		fi.LastSeen = lastSeen

		ip, ipErr := m.ctx.Toxp.FriendGetConnectionIP(fid)
		if ipErr == nil {
			fi.PeerIp = ip
		}

		result = append(result, fi)
	}
	return result
}

// ── Messages ──

func (m *Midapi) SendFriendMessage(friendID uint32, message string) (uint32, error) {
	msgID, err := m.ctx.Tox.FriendSendMessage(friendID, message)
	if err != nil {
		return 0, err
	}

	friendPubKey, _ := m.ctx.Tox.FriendGetPublicKey(friendID)
	chanidInt, _ := m.getOrCreatePubKeyID(friendPubKey)
	selfPubKey := m.ctx.Tox.SelfGetPublicKey()
	senderInt, _ := m.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	m.persistEventToSQLite(chanidInt, string(data))

	return msgID, nil
}

func (m *Midapi) SendGroupMessage(gn uint32, messageType string, message string) (uint64, error) {
	mt := tox.MESSAGE_TYPE_NORMAL
	if messageType == "action" {
		mt = tox.MESSAGE_TYPE_ACTION
	}
	gn2 := tox.GroupNumber(gn)
	msgId, err := m.ctx.Tox.GroupSendMessage(gn2, mt, message)
	if err != nil {
		return 0, err
	}

	chatId, _ := m.ctx.Tox.GroupGetChatId(gn2)
	chanidInt, _ := m.getOrCreatePubKeyID(chatId)
	selfPubKey := m.ctx.Tox.SelfGetPublicKey()
	senderInt, _ := m.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	m.persistEventToSQLite(chanidInt, string(data))

	return uint64(msgId), nil
}

func (m *Midapi) SendConferenceMessage(confID uint32, message string) error {
	_, err := m.ctx.Tox.ConferenceSendMessage(confID, tox.MESSAGE_TYPE_NORMAL, message)
	if err != nil {
		return err
	}

	chatId, _ := m.ctx.Tox.ConferenceGetIdentifier(confID)
	chanidInt, _ := m.getOrCreatePubKeyID(chatId)
	selfPubKey := m.ctx.Tox.SelfGetPublicKey()
	senderInt, _ := m.getOrCreatePubKeyID(selfPubKey)
	data, _ := json.Marshal(map[string]interface{}{
		"message":   message,
		"sender":    senderInt,
		"direction": "sent",
	})
	m.persistEventToSQLite(chanidInt, string(data))

	return nil
}

// ── Groups ──

func (m *Midapi) GroupsList() []GroupInfo {
	numGroups := m.ctx.Tox.GroupGetNumberGroups()
	groups := make([]GroupInfo, 0, numGroups)
	for i := uint32(0); i < numGroups; i++ {
		gn := tox.GroupNumber(i)
		g := GroupInfo{
			GroupNumber: int(gn),
		}
		if name, err := m.ctx.Tox.GroupGetName(gn); err == nil {
			g.GroupName = name
		}
		if chatId, err := m.ctx.Tox.GroupGetChatId(gn); err == nil {
			g.ChatId = chatId
		}
		if connected, err := m.ctx.Tox.GroupIsConnected(gn); err == nil {
			g.IsConnected = connected
		}
		if topic, err := m.ctx.Tox.GroupGetTopic(gn); err == nil {
			g.StatusText = topic
		}
		g.MemberCount = m.groupPeerCount(gn)
		groups = append(groups, g)
	}
	return groups
}

func (m *Midapi) GroupCreate(privacyState, groupName, creatorName, password string) (uint32, error) {
	if groupName == "" {
		return 0, fmt.Errorf("missing group_name")
	}
	var ps tox.GroupPrivacyState
	if privacyState == "private" {
		ps = tox.GroupPrivacyState(tox.GROUP_PRIVACY_STATE_PRIVATE)
	} else {
		ps = tox.GroupPrivacyState(tox.GROUP_PRIVACY_STATE_PUBLIC)
	}
	gn, err := m.ctx.Tox.GroupNew(ps, groupName, creatorName)
	if err != nil {
		return 0, err
	}
	if password != "" {
		if err := m.ctx.Tox.GroupSetPassword(gn, password); err != nil {
			return 0, fmt.Errorf("set password failed: %s", err)
		}
	}
	m.requestSave()
	return uint32(gn), nil
}

func (m *Midapi) GroupJoin(chatId, name, password string) (uint32, error) {
	if name == "" {
		name = m.ctx.Tox.SelfGetName()
		if name == "" {
			pubkey := m.ctx.Tox.SelfGetPublicKey()
			if len(pubkey) >= 5 {
				name = "nonamed" + pubkey[:5]
			} else {
				name = "nonamed"
			}
		}
		log.Printf("[GroupJoin] name auto-filled: %s", name)
	}
	gn, err := m.ctx.Tox.GroupJoin(chatId, name, password)
	if err != nil {
		errStr := err.Error()
		log.Printf("[GroupJoin] FAILED: chat_id=%s..., error=%s", safeTruncate(chatId, 16), errStr)
		if strings.Contains(errStr, "chat id invalid") || strings.Contains(errStr, "3") {
			return 0, fmt.Errorf("无效的群组ID：群组不存在、已过期或需要密码")
		} else if strings.Contains(errStr, "bad password") {
			return 0, fmt.Errorf("密码错误")
		} else if strings.Contains(errStr, "failed to decrypt") {
			return 0, fmt.Errorf("群组ID解密失败，可能已损坏")
		}
		return 0, err
	}
	log.Printf("[GroupJoin] SUCCESS: group_number=%d, chat_id=%s...", uint32(gn), safeTruncate(chatId, 16))
	m.requestSave()
	return uint32(gn), nil
}

func (m *Midapi) GroupLeave(gn uint32, partMessage string) error {
	err := m.ctx.Tox.GroupLeave(tox.GroupNumber(gn), partMessage)
	if err != nil {
		return err
	}
	m.requestSave()
	return nil
}

func (m *Midapi) GroupInvite(gn uint32, friendNumber uint32) error {
	return m.ctx.Tox.GroupInviteFriend(tox.GroupNumber(gn), friendNumber)
}

func (m *Midapi) GroupAccept(inviteData string, friendNumber uint32, name, password string) (uint32, error) {
	name = m.getDefaultName(name)
	gn, err := m.ctx.Tox.GroupInviteAccept(inviteData, friendNumber, name, password)
	if err != nil {
		return 0, err
	}
	return uint32(gn), nil
}

func (m *Midapi) GroupSetName(gn uint32, name string) error {
	err := m.ctx.Tox.GroupSelfSetName(tox.GroupNumber(gn), name)
	if err != nil {
		return err
	}
	m.requestSave()
	return nil
}

func (m *Midapi) GroupSetTopic(gn uint32, topic string) error {
	err := m.ctx.Tox.GroupSetTopic(tox.GroupNumber(gn), topic)
	if err != nil {
		return err
	}
	m.requestSave()
	return nil
}

func (m *Midapi) GroupMembers(gn uint32) *GroupMemberList {
	gn2 := tox.GroupNumber(gn)
	members := make([]GroupMember, 0)
	for peerNumber := 0; peerNumber < 256; peerNumber++ {
		pn := tox.GroupPeerNumber(peerNumber)
		name, err := m.ctx.Tox.GroupPeerGetName(gn2, pn)
		if err != nil {
			break
		}
		pubKey, _ := m.ctx.Tox.GroupPeerGetPublicKey(gn2, pn)
		connStatus, _ := m.ctx.Tox.GroupPeerGetConnectionStatus(gn2, pn)
		role, _ := m.ctx.Tox.GroupPeerGetRole(gn2, pn)
		ip, ipErr := m.ctx.Toxp.GroupPeerGetIPAddress(gn, uint32(pn))
		if ipErr != nil {
			log.Printf("[IP] Group %d Peer %d: error: %v", int(gn2), peerNumber, ipErr)
		}
		member := GroupMember{
			PeerNumber: peerNumber,
			Name:       name,
			IconUrl:    "",
			Status:     connStatus,
			StatusStr:  groupStatusToStr(connStatus),
			StatusText: "",
			Role:       int(role),
			RoleStr:    roleToStr(int(role)),
			PublicKey:  pubKey,
			IsSelf:     false,
			LastSeen:   nil,
			PeerIp:     ip,
		}
		members = append(members, member)
	}

	selfPeerNumber, selfErr := m.ctx.Tox.GroupSelfGetPeerId(gn2)
	for i := range members {
		if members[i].PeerNumber == int(selfPeerNumber) {
			members[i].IsSelf = true
		}
	}
	sn := int(selfPeerNumber)
	if selfErr != nil {
		sn = 0
	}
	return &GroupMemberList{
		GroupNumber:    int(gn2),
		Members:        members,
		SelfPeerNumber: sn,
	}
}

func (m *Midapi) groupPeerCount(gn tox.GroupNumber) int {
	for peerNum := 0; peerNum < 256; peerNum++ {
		_, err := m.ctx.Tox.GroupPeerGetName(gn, tox.GroupPeerNumber(peerNum))
		if err != nil {
			return peerNum
		}
	}
	return 256
}

func (m *Midapi) ChanIDForContact(contactID uint32, contactType string) (string, error) {
	switch contactType {
	case "friend":
		return m.ctx.Tox.FriendGetPublicKey(contactID)
	case "group":
		return m.ctx.Tox.GroupGetChatId(tox.GroupNumber(contactID))
	case "conference":
		return m.ctx.Tox.ConferenceGetIdentifier(contactID)
	default:
		return "", fmt.Errorf("unknown contact_type: %s", contactType)
	}
}

// ── Conferences ──

func (m *Midapi) ConferencesList() []ConferenceInfo {
	confIDs := m.ctx.Tox.ConferenceGetChatlist()
	conferences := make([]ConferenceInfo, 0, len(confIDs))
	for _, confID := range confIDs {
		c := ConferenceInfo{
			ConferenceNumber: confID,
		}
		if title, err := m.ctx.Tox.ConferenceGetTitle(confID); err == nil {
			c.ConferenceName = title
			c.StatusText = title
		}
		if chatId, err := m.ctx.Tox.ConferenceGetIdentifier(confID); err == nil {
			c.ChatId = chatId
		}
		m.ctx.Mu.RLock()
		if connected, ok := m.ctx.ConferenceConnected[confID]; ok {
			c.IsConnected = connected
		}
		m.ctx.Mu.RUnlock()
		c.MemberCount = int(m.ctx.Tox.ConferencePeerCount(confID))
		conferences = append(conferences, c)
	}
	return conferences
}

func (m *Midapi) ConferenceCreate() (uint32, error) {
	confID, err := m.ctx.Tox.ConferenceNew()
	if err != nil {
		return 0, err
	}
	m.requestSave()
	return confID, nil
}

func (m *Midapi) ConferenceJoin(friendNumber uint32, cookie string) (uint32, error) {
	confID, err := m.ctx.Tox.ConferenceJoin(friendNumber, cookie)
	if err != nil {
		return 0, err
	}
	log.Printf("[TOX] Successfully joined conference %d from friend %d", confID, friendNumber)
	m.requestSave()
	return confID, nil
}

func (m *Midapi) ConferenceSetTitle(confID uint32, title string) error {
	_, err := m.ctx.Tox.ConferenceSetTitle(confID, title)
	if err != nil {
		return err
	}
	m.requestSave()
	return nil
}

func (m *Midapi) ConferenceLeave(confID uint32) error {
	_, err := m.ctx.Tox.ConferenceDelete(confID)
	if err != nil {
		return err
	}
	m.requestSave()
	return nil
}

func (m *Midapi) ConferenceMembers(confID uint32) *ConferenceMemberList {
	peers := m.ctx.Tox.ConferenceGetPeers(confID)
	members := make([]ConferenceMember, 0, len(peers))
	for peerNumber := range peers {
		name, err := m.ctx.Tox.ConferencePeerGetName(confID, peerNumber)
		if err != nil {
			name = fmt.Sprintf("Peer %d", peerNumber)
		}
		members = append(members, ConferenceMember{
			PeerNumber: int(peerNumber),
			Name:       name,
		})
	}
	return &ConferenceMemberList{
		ConferenceID: int(confID),
		Members:      members,
	}
}

// ── Events ──

func (m *Midapi) EventsPoll(after uint64) (events []Event, nextID uint64) {
	return m.ctx.EventQueue.PopAfter(after)
}

func (m *Midapi) EventsDelete(id uint64) {
	m.ctx.EventQueue.DeleteEvent(id)
}

func (m *Midapi) ToxIterate(duration time.Duration) int {
	deadline := time.Now().Add(duration)
	count := 0
	ms20 := 20 * time.Millisecond
	for time.Now().Before(deadline) {
		m.ctx.Tox.Iterate()
		m.ctx.checkRebootstrap()
		count++
		time.Sleep(ms20 + time.Duration(m.ctx.Tox.IterationInterval())*time.Millisecond)
	}
	return count
}

// ── Bootstrap ──

func (m *Midapi) BootstrapAll() {
	bootstrapAll(m.ctx.Tox)
}

// ── Random name ──

func (m *Midapi) RandomName() string {
	return randomName()
}

// ── Translate ──

func (m *Midapi) Translate(text, to string) (string, error) {
	results, err := oai.MsetTranFull(to, "", text)
	if err != nil {
		return "", err
	}
	if len(results) == 0 {
		return "", fmt.Errorf("translation returned empty result")
	}
	return results[0], nil
}

// ── Message history ──

func (m *Midapi) MessageHistory(chanid, contactType string) ([]MessageRecord, error) {
	if contactType == "" {
		return nil, fmt.Errorf("missing contact_type")
	}

	var chanidInt int64
	err := m.ctx.DB.QueryRow("SELECT pkid FROM pubkey_ids WHERE pubkey = ?", chanid).Scan(&chanidInt)
	if err != nil {
		return nil, fmt.Errorf("invalid chanid")
	}

	rows, err := m.ctx.DB.Query(`SELECT rowid, data, created_at FROM events WHERE chanid = ? ORDER BY rowid DESC LIMIT 50`, chanidInt)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	messages := make([]MessageRecord, 0)
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
			if err := m.ctx.DB.QueryRow("SELECT pubkey FROM pubkey_ids WHERE pkid = ?", int64(sender)).Scan(&pk); err == nil {
				senderPubkey = pk
			}
		}

		senderNumber := uint32(ContactNotFound)
		if senderPubkey != "" {
			senderNumber, _ = m.getContactNumber(contactType, chanid, senderPubkey)
		}

		messages = append(messages, MessageRecord{
			RowID:        rowid,
			Message:      eventData["message"],
			SenderPubkey: senderPubkey,
			SenderNumber: senderNumber,
			Direction:    eventData["direction"],
			CreatedAt:    createdAt,
		})
	}

	for i, j := 0, len(messages)-1; i < j; i, j = i+1, j-1 {
		messages[i], messages[j] = messages[j], messages[i]
	}

	return messages, nil
}

// ── getDefaultName (helper) ──

func (m *Midapi) getDefaultName(name string) string {
	if name != "" {
		return name
	}
	selfName := m.ctx.Tox.SelfGetName()
	if selfName != "" {
		return selfName
	}
	pubkey := m.ctx.Tox.SelfGetPublicKey()
	if len(pubkey) >= 7 {
		return "nonamed." + pubkey[:7]
	}
	return "nonamed"
}

// ── getContactNumber (helper) ──

func (m *Midapi) getContactNumber(contactType string, contactPubkey string, senderPubkey string) (uint32, error) {
	switch contactType {
	case "friend":
		num, err := m.ctx.Tox.FriendByPublicKey(senderPubkey)
		if err != nil {
			return ContactNotFound, nil
		}
		return num, nil

	case "group":
		groupNum, err := m.ctx.Tox.GroupByChatId(contactPubkey)
		if err != nil {
			return ContactNotFound, nil
		}
		for i := uint32(0); i < 100; i++ {
			peerPubkey, err := m.ctx.Tox.GroupPeerGetPublicKey(groupNum, tox.GroupPeerNumber(i))
			if err != nil {
				continue
			}
			if strings.EqualFold(peerPubkey, senderPubkey) {
				return i, nil
			}
		}
		return ContactNotFound, nil

	case "conference":
		chatlist := m.ctx.Tox.ConferenceGetChatlist()
		for _, confNum := range chatlist {
			identifier, err := m.ctx.Tox.ConferenceGetIdentifier(confNum)
			if err != nil {
				continue
			}
			if strings.EqualFold(identifier, contactPubkey) {
				for j := uint32(0); j < 100; j++ {
					peerPubkey, err := m.ctx.Tox.ConferencePeerGetPublicKey(confNum, j)
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

// ── Storage helpers ──

func (m *Midapi) getOrCreatePubKeyID(pubkey string) (int64, error) {
	var pkid int64
	err := m.ctx.DB.QueryRow(`SELECT pkid FROM pubkey_ids WHERE pubkey = ?`, pubkey).Scan(&pkid)
	if err == nil {
		return pkid, nil
	}
	if err != sql.ErrNoRows {
		return 0, err
	}
	result, err := m.ctx.DB.Exec(`INSERT INTO pubkey_ids(pubkey) VALUES(?)`, pubkey)
	if err != nil {
		return 0, err
	}
	return result.LastInsertId()
}

func (m *Midapi) persistEventToSQLite(chanid int64, data string) {
	if _, err := m.ctx.DB.Exec("INSERT INTO events(chanid, data) VALUES(?, ?)", chanid, data); err != nil {
		log.Printf("Failed to persist event to SQLite: %v", err)
	}
}

// ── Setup callbacks ──

func (m *Midapi) SetupCallbacks() {
	s := m.ctx
	s.Tox.CallbackSelfConnectionStatus(func(this *tox.Tox, status int, userData interface{}) {
		s.Mu.Lock()
		defer s.Mu.Unlock()
		var statusStr string
		switch status {
		case tox.CONNECTION_NONE:
			s.SelfConnectionStatus = "offline"
			statusStr = "OFFLINE"
		case tox.CONNECTION_TCP:
			s.SelfConnectionStatus = "tcp"
			statusStr = "TCP"
		case tox.CONNECTION_UDP:
			s.SelfConnectionStatus = "udp"
			statusStr = "UDP"
		default:
			s.SelfConnectionStatus = "unknown"
			statusStr = "UNKNOWN"
		}
		log.Printf("[TOX_CALLBACK] SelfConnectionStatus: %s (%d)", statusStr, status)
		data, _ := json.Marshal(map[string]interface{}{"status": s.SelfConnectionStatus})
		s.EventQueue.Push("self_connection_status", string(data))
	}, nil)

	s.Tox.CallbackFriendRequest(func(this *tox.Tox, pubkey string, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendRequest: pubkey=%s, message=%s", pubkey, message)
		_, err := s.Tox.FriendAddNorequest(pubkey)
		if err != nil {
			log.Printf("Failed to accept friend request: %v", err)
		} else {
			data, _ := json.Marshal(map[string]interface{}{"public_key": pubkey})
			s.EventQueue.Push("friend_request", string(data))
			m.requestSave()
		}
	}, nil)

	s.Tox.CallbackFriendMessage(func(this *tox.Tox, friendNumber uint32, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendMessage: friend=%d, message=%s", friendNumber, message)
		friendPubKey, _ := s.Tox.FriendGetPublicKey(friendNumber)
		chanidInt, _ := m.getOrCreatePubKeyID(friendPubKey)
		senderInt, _ := m.getOrCreatePubKeyID(friendPubKey)
		data, _ := json.Marshal(map[string]interface{}{
			"message":   message,
			"sender":    senderInt,
			"direction": "received",
			"friend_id": friendNumber,
		})
		s.EventQueue.Push("friend_message", string(data))
		m.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	s.Tox.CallbackFriendConnectionStatus(func(this *tox.Tox, friendNumber uint32, connectionStatus int, userData interface{}) {
		s.Mu.Lock()
		defer s.Mu.Unlock()
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
		s.FriendStatuses[friendNumber] = status
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"status":    status,
		})
		s.EventQueue.Push("friend_connection_status", string(data))
	}, nil)

	s.Tox.CallbackFriendName(func(this *tox.Tox, friendNumber uint32, newName string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendName: friend=%d, name=%s", friendNumber, newName)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"name":      newName,
		})
		s.EventQueue.Push("friend_name", string(data))
		m.requestSave()
	}, nil)

	s.Tox.CallbackFriendStatusMessage(func(this *tox.Tox, friendNumber uint32, newStatus string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendStatusMessage: friend=%d, status=%s", friendNumber, newStatus)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id":      friendNumber,
			"status_message": newStatus,
		})
		s.EventQueue.Push("friend_status_message", string(data))
		m.requestSave()
	}, nil)

	s.Tox.CallbackFriendStatus(func(this *tox.Tox, friendNumber uint32, status int, userData interface{}) {
		s.Mu.Lock()
		s.FriendUserStatuses[friendNumber] = status
		s.Mu.Unlock()
		log.Printf("[TOX_CALLBACK] FriendStatus: friend=%d, status=%d", friendNumber, status)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"status":    status,
		})
		s.EventQueue.Push("friend_status", string(data))
	}, nil)

	s.Tox.CallbackFriendTyping(func(this *tox.Tox, friendNumber uint32, isTyping uint8, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendTyping: friend=%d, typing=%v", friendNumber, isTyping)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"typing":    isTyping,
		})
		s.EventQueue.Push("friend_typing", string(data))
	}, nil)

	s.Tox.CallbackFriendReadReceipt(func(this *tox.Tox, friendNumber uint32, receipt uint32, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FriendReadReceipt: friend=%d, receipt=%d", friendNumber, receipt)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_id": friendNumber,
			"receipt":   receipt,
		})
		s.EventQueue.Push("friend_read_receipt", string(data))
	}, nil)

	s.Tox.CallbackFileRecvControl(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, control int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecvControl: friend=%d, file=%d, control=%d", friendNumber, fileNumber, control)
	}, nil)

	s.Tox.CallbackFileRecv(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, kind uint32, fileSize uint64, fileName string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecv: friend=%d, file=%d, kind=%d, size=%d, name=%s", friendNumber, fileNumber, kind, fileSize, fileName)
	}, nil)

	s.Tox.CallbackFileRecvChunk(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, position uint64, data []byte, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileRecvChunk: friend=%d, file=%d, position=%d, len=%d", friendNumber, fileNumber, position, len(data))
	}, nil)

	s.Tox.CallbackFileChunkRequest(func(this *tox.Tox, friendNumber uint32, fileNumber uint32, position uint64, length int, userData interface{}) {
		log.Printf("[TOX_CALLBACK] FileChunkRequest: friend=%d, file=%d, position=%d, length=%d", friendNumber, fileNumber, position, length)
	}, nil)

	s.Tox.CallbackConferenceInvite(func(this *tox.Tox, friendNumber uint32, itype uint8, cookie string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceInvite: friend=%d, type=%d, cookie=%s", friendNumber, itype, cookie)
		data, _ := json.Marshal(map[string]interface{}{
			"friend_number": friendNumber,
			"type":          itype,
			"cookie":        cookie,
		})
		s.EventQueue.Push("conference_invite", string(data))
	}, nil)

	s.Tox.CallbackConferenceMessage(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceMessage: group=%d, peer=%d, message=%s", groupNumber, peerNumber, message)
		chatId, _ := s.Tox.ConferenceGetIdentifier(groupNumber)
		chanidInt, _ := m.getOrCreatePubKeyID(chatId)
		peerPubKey, _ := s.Tox.ConferencePeerGetPublicKey(groupNumber, peerNumber)
		senderInt, _ := m.getOrCreatePubKeyID(peerPubKey)
		peerName, _ := s.Tox.ConferencePeerGetName(groupNumber, peerNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"message":           message,
			"sender":            senderInt,
			"direction":         "received",
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
			"peer_name":         peerName,
		})
		s.EventQueue.Push("conference_message", string(data))
		m.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	s.Tox.CallbackGroupMessage(func(this *tox.Tox, groupNumber tox.GroupNumber, peerNumber tox.GroupPeerNumber, message string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] GroupMessage: group=%d, peer=%d, message=%s", groupNumber, peerNumber, message)
		chatId, _ := s.Tox.GroupGetChatId(groupNumber)
		chanidInt, _ := m.getOrCreatePubKeyID(chatId)
		peerPubKey, _ := s.Tox.GroupPeerGetPublicKey(groupNumber, peerNumber)
		senderInt, _ := m.getOrCreatePubKeyID(peerPubKey)
		peerName, _ := s.Tox.GroupPeerGetName(groupNumber, peerNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"message":      message,
			"sender":       senderInt,
			"direction":    "received",
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"peer_name":    peerName,
		})
		s.EventQueue.Push("group_message", string(data))
		m.persistEventToSQLite(chanidInt, string(data))
	}, nil)

	s.Tox.CallbackConferenceTitle(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, title string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferenceTitle: group=%d, peer=%d, title=%s", groupNumber, peerNumber, title)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
			"title":             title,
		})
		s.EventQueue.Push("conference_title", string(data))
		m.requestSave()
	}, nil)

	s.Tox.CallbackConferencePeerName(func(this *tox.Tox, groupNumber uint32, peerNumber uint32, name string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferencePeerName: group=%d, peer=%d, name=%s", groupNumber, peerNumber, name)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"peer_number":       peerNumber,
			"name":              name,
		})
		s.EventQueue.Push("conference_peer_name", string(data))
	}, nil)

	s.Tox.CallbackConferencePeerListChanged(func(this *tox.Tox, groupNumber uint32, userData interface{}) {
		log.Printf("[TOX_CALLBACK] ConferencePeerListChanged: group=%d", groupNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
		})
		s.EventQueue.Push("conference_peer_list_changed", string(data))
	}, nil)

	s.Tox.CallbackGroupInvite(func(this *tox.Tox, groupNumber tox.GroupNumber,
		friendNumber uint32, data string, userData interface{}) {
		log.Printf("[GroupInvite] group=%d, friend=%d, data=%s", groupNumber, friendNumber, data)
		if data == "" {
			log.Printf("[GroupInvite] WARNING: empty invite data from friend %d, skipping event", friendNumber)
			return
		}
		eventData, _ := json.Marshal(map[string]interface{}{
			"friend_number": friendNumber,
			"chat_id":       data,
		})
		s.EventQueue.Push("group_invite", string(eventData))
		log.Printf("[GroupInvite] Pushed event: friend=%d, chat_id=%s", friendNumber, data)
	}, nil)

	s.Tox.CallbackGroupSelfJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		userData interface{}) {
		log.Printf("[GroupSelfJoin] group=%d", groupNumber)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": groupNumber,
		})
		s.EventQueue.Push("group_self_join", string(data))
	}, nil)

	s.Tox.CallbackGroupPeerJoin(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, userData interface{}) {
		log.Printf("[GroupPeerJoin] group=%d, peer=%d", int(groupNumber), int(peerNumber))
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
		})
		s.EventQueue.Push("group_peer_join", string(data))
	}, nil)

	s.Tox.CallbackGroupPeerExit(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, exitType tox.GroupExitType, name string, userData interface{}) {
		log.Printf("[GroupPeerExit] group=%d, peer=%d, type=%s, name=%s",
			int(groupNumber), int(peerNumber), tox.GroupExitTypeToString(exitType), name)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"exit_type":    tox.GroupExitTypeToString(exitType),
			"name":         name,
		})
		s.EventQueue.Push("group_peer_exit", string(data))
	}, nil)

	s.Tox.CallbackGroupPeerStatus(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, status int, userData interface{}) {
		log.Printf("[GroupPeerStatus] group=%d, peer=%d, status=%d", int(groupNumber), int(peerNumber), status)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"status":       status,
		})
		s.EventQueue.Push("group_peer_status", string(data))
	}, nil)

	s.Tox.CallbackGroupJoinFail(func(this *tox.Tox, groupNumber tox.GroupNumber,
		failType tox.GroupJoinFail, userData interface{}) {
		failStr := tox.GroupJoinFailToString(failType)
		log.Printf("[GroupJoinFail] group=%d, error=%d (%s)", int(groupNumber), int(failType), failStr)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"error":        failStr,
		})
		s.EventQueue.Push("group_join_fail", string(data))
	}, nil)

	s.Tox.CallbackGroupPeerName(func(this *tox.Tox, groupNumber tox.GroupNumber,
		peerNumber tox.GroupPeerNumber, name string, userData interface{}) {
		log.Printf("[GroupPeerName] group=%d, peer=%d, name=%s", int(groupNumber), int(peerNumber), name)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"name":         name,
		})
		s.EventQueue.Push("group_peer_name", string(data))
	}, nil)

	s.Tox.CallbackConferenceConnected(func(this *tox.Tox, groupNumber uint32, userData interface{}) {
		log.Printf("[ConferenceConnected] group=%d", groupNumber)
		s.Mu.Lock()
		s.ConferenceConnected[groupNumber] = true
		s.Mu.Unlock()
		data, _ := json.Marshal(map[string]interface{}{
			"conference_number": groupNumber,
			"is_connected":      true,
		})
		s.EventQueue.Push("conference_connected", string(data))
	}, nil)

	s.Tox.CallbackGroupTopic(func(this *tox.Tox, groupNumber tox.GroupNumber, peerNumber tox.GroupPeerNumber, topic string, userData interface{}) {
		log.Printf("[TOX_CALLBACK] GroupTopic: group=%d, peer=%d, topic=%s", int(groupNumber), int(peerNumber), topic)
		data, _ := json.Marshal(map[string]interface{}{
			"group_number": int(groupNumber),
			"peer_number":  int(peerNumber),
			"topic":        topic,
		})
		s.EventQueue.Push("group_topic", string(data))
		m.requestSave()
	}, nil)

	log.Println("[TOX] All callbacks registered")
}
