// Package tox provides Go bindings for toxcore using cgo
package tox

/*
#cgo CFLAGS: -I/opt/oldlibc-devsys/include
#cgo LDFLAGS: -L/opt/oldlibc-devsys/lib -ltoxcore -ltoxencryptsave -lsodium -lpthread

#include <tox/tox.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations for Go callbacks
extern void go_friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data);
extern void go_friend_message_cb(Tox *tox, uint32_t friend_id, Tox_Message_Type type, const uint8_t *message, size_t length, void *user_data);
extern void go_friend_name_cb(Tox *tox, uint32_t friend_id, const uint8_t *name, size_t length, void *user_data);
extern void go_friend_status_cb(Tox *tox, uint32_t friend_id, Tox_User_Status status, void *user_data);
extern void go_friend_status_msg_cb(Tox *tox, uint32_t friend_id, const uint8_t *message, size_t length, void *user_data);
extern void go_friend_connection_status_cb(Tox *tox, uint32_t friend_id, Tox_Connection connection_status, void *user_data);
extern void go_self_connection_status_cb(Tox *tox, Tox_Connection connection_status, void *user_data);
extern void go_friend_typing_cb(Tox *tox, uint32_t friend_id, bool is_typing, void *user_data);
extern void go_conference_invite_cb(Tox *tox, uint32_t friend_id, Tox_Conference_Type type, const uint8_t *conference_pubkey, size_t length, void *user_data);
extern void go_conference_message_cb(Tox *tox, Tox_Conference_Number conference_number, Tox_Friend_Number peer_number, Tox_Message_Type type, const uint8_t *message, size_t length, void *user_data);

// C wrapper functions that call Go callbacks
static void c_friend_request_cb(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data) {
    go_friend_request_cb(tox, public_key, message, length, user_data);
}

static void c_friend_message_cb(Tox *tox, uint32_t friend_id, Tox_Message_Type type, const uint8_t *message, size_t length, void *user_data) {
    go_friend_message_cb(tox, friend_id, type, message, length, user_data);
}

static void c_friend_name_cb(Tox *tox, uint32_t friend_id, const uint8_t *name, size_t length, void *user_data) {
    go_friend_name_cb(tox, friend_id, name, length, user_data);
}

static void c_friend_status_cb(Tox *tox, uint32_t friend_id, Tox_User_Status status, void *user_data) {
    go_friend_status_cb(tox, friend_id, status, user_data);
}

static void c_friend_status_msg_cb(Tox *tox, uint32_t friend_id, const uint8_t *message, size_t length, void *user_data) {
    go_friend_status_msg_cb(tox, friend_id, message, length, user_data);
}

static void c_friend_connection_status_cb(Tox *tox, uint32_t friend_id, Tox_Connection connection_status, void *user_data) {
    go_friend_connection_status_cb(tox, friend_id, connection_status, user_data);
}

static void c_self_connection_status_cb(Tox *tox, Tox_Connection connection_status, void *user_data) {
    go_self_connection_status_cb(tox, connection_status, user_data);
}

static void c_friend_typing_cb(Tox *tox, uint32_t friend_id, bool is_typing, void *user_data) {
    go_friend_typing_cb(tox, friend_id, is_typing, user_data);
}

static void c_conference_invite_cb(Tox *tox, uint32_t friend_id, Tox_Conference_Type type, const uint8_t *conference_pubkey, size_t length, void *user_data) {
    go_conference_invite_cb(tox, friend_id, type, conference_pubkey, length, user_data);
}

static void c_conference_message_cb(Tox *tox, Tox_Conference_Number conference_number, Tox_Friend_Number peer_number, Tox_Message_Type type, const uint8_t *message, size_t length, void *user_data) {
    go_conference_message_cb(tox, conference_number, peer_number, type, message, length, user_data);
}

// Register all callbacks
static void register_callbacks(Tox *tox) {
    tox_callback_friend_request(tox, c_friend_request_cb);
    tox_callback_friend_message(tox, c_friend_message_cb);
    tox_callback_friend_name(tox, c_friend_name_cb);
    tox_callback_friend_status(tox, c_friend_status_cb);
    tox_callback_friend_status_message(tox, c_friend_status_msg_cb);
    tox_callback_friend_connection_status(tox, c_friend_connection_status_cb);
    tox_callback_self_connection_status(tox, c_self_connection_status_cb);
    tox_callback_friend_typing(tox, c_friend_typing_cb);
    tox_callback_conference_invite(tox, c_conference_invite_cb);
    tox_callback_conference_message(tox, c_conference_message_cb);
}
*/
import "C"

import (
	"fmt"
	"github.com/anomalyco/toxhttpd-go/log"
	"os"
	"path/filepath"
	"unsafe"
)

type Tox struct {
	tox *C.Tox
}

//export go_friend_request_cb
func go_friend_request_cb(tox *C.Tox, publicKey *C.uint8_t, message *C.uint8_t, length C.size_t, userData unsafe.Pointer) {
	pk := (*[32]byte)(unsafe.Pointer(publicKey))
	msg := C.GoStringN((*C.char)(unsafe.Pointer(message)), C.int(length))
	log.Info("Tox: friend request from %x, msg: %s", pk, msg)
}

//export go_friend_message_cb
func go_friend_message_cb(tox *C.Tox, friendID C.uint32_t, msgType C.Tox_Message_Type, message *C.uint8_t, length C.size_t, userData unsafe.Pointer) {
	msg := C.GoStringN((*C.char)(unsafe.Pointer(message)), C.int(length))
	log.Info("Tox: friend message from %d: %s", friendID, msg)
}

//export go_friend_name_cb
func go_friend_name_cb(tox *C.Tox, friendID C.uint32_t, name *C.uint8_t, length C.size_t, userData unsafe.Pointer) {
	n := C.GoStringN((*C.char)(unsafe.Pointer(name)), C.int(length))
	log.Info("Tox: friend %d name changed: %s", friendID, n)
}

//export go_friend_status_cb
func go_friend_status_cb(tox *C.Tox, friendID C.uint32_t, status C.Tox_User_Status, userData unsafe.Pointer) {
	log.Info("Tox: friend %d status changed: %d", friendID, status)
}

//export go_friend_status_msg_cb
func go_friend_status_msg_cb(tox *C.Tox, friendID C.uint32_t, message *C.uint8_t, length C.size_t, userData unsafe.Pointer) {
	msg := C.GoStringN((*C.char)(unsafe.Pointer(message)), C.int(length))
	log.Info("Tox: friend %d status message: %s", friendID, msg)
}

//export go_friend_connection_status_cb
func go_friend_connection_status_cb(tox *C.Tox, friendID C.uint32_t, conn C.Tox_Connection, userData unsafe.Pointer) {
	log.Info("Tox: friend %d connection status: %d", friendID, conn)
}

//export go_self_connection_status_cb
func go_self_connection_status_cb(tox *C.Tox, conn C.Tox_Connection, userData unsafe.Pointer) {
	log.Info("Tox: self connection status: %d", conn)
}

//export go_friend_typing_cb
func go_friend_typing_cb(tox *C.Tox, friendID C.uint32_t, isTyping C.bool, userData unsafe.Pointer) {
	log.Info("Tox: friend %d typing: %v", friendID, isTyping)
}

//export go_conference_invite_cb
func go_conference_invite_cb(tox *C.Tox, friendID C.uint32_t, confType C.Tox_Conference_Type, confPubKey *C.uint8_t, length C.size_t, userData unsafe.Pointer) {
	log.Info("Tox: conference invite from friend %d", friendID)
}

//export go_conference_message_cb
func go_conference_message_cb(tox *C.Tox, confNum C.Tox_Conference_Number, peerNum C.Tox_Friend_Number, msgType C.Tox_Message_Type, message *C.uint8_t, length C.size_t, userData unsafe.Pointer) {
	msg := C.GoStringN((*C.char)(unsafe.Pointer(message)), C.int(length))
	log.Info("Tox: conference %d message from peer %d: %s", confNum, peerNum, msg)
}

func NewTox() (*Tox, error) {
	// Try to load saved data
	data, err := os.ReadFile("data/savedata.bin")
	var opts *C.Tox_Options

	if err == nil && len(data) > 100 {
		opts = C.tox_options_new(nil)
		C.tox_options_set_savedata_type(opts, C.TOX_SAVEDATA_TYPE_TOX_SAVE)
		C.tox_options_set_savedata_data(opts, (*C.uint8_t)(unsafe.Pointer(&data[0])), C.size_t(len(data)))
	}

	var errNew C.Tox_Err_New
	var t *C.Tox
	if opts != nil {
		t = C.tox_new(opts, &errNew)
		C.tox_options_free(opts)
	} else {
		t = C.tox_new(nil, &errNew)
	}

	if errNew != C.TOX_ERR_NEW_OK {
		return nil, fmt.Errorf("failed to create tox: %d", errNew)
	}

	// Register callbacks
	C.register_callbacks(t)

	return &Tox{tox: t}, nil
}

func (t *Tox) Save() error {
	size := C.tox_get_savedata_size(t.tox)
	data := make([]byte, size)
	C.tox_get_savedata(t.tox, (*C.uint8_t)(unsafe.Pointer(&data[0])))
	
	// Create data directory if not exists
	os.MkdirAll("data", 0700)
	
	return os.WriteFile(filepath.Join("data", "savedata.bin"), data, 0600)
}

func (t *Tox) Close() {
	// Save before close
	t.Save()
	C.tox_kill(t.tox)
}

func (t *Tox) GetAddress() string {
	var address [C.TOX_ADDRESS_SIZE]byte
	C.tox_self_get_address(t.tox, (*C.uint8_t)(unsafe.Pointer(&address[0])))
	hex := ""
	for _, b := range address {
		hex += fmt.Sprintf("%02x", b)
	}
	return hex
}

func (t *Tox) GetSelfName() string {
	size := C.tox_self_get_name_size(t.tox)
	if size == 0 {
		return ""
	}
	name := make([]byte, size)
	C.tox_self_get_name(t.tox, (*C.uint8_t)(unsafe.Pointer(&name[0])))
	return string(name)
}

func (t *Tox) SetSelfName(name string) error {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	var err C.Tox_Err_Set_Info
	C.tox_self_set_name(t.tox, (*C.uint8_t)(unsafe.Pointer(cname)), C.size_t(len(name)), &err)
	if err != C.TOX_ERR_SET_INFO_OK {
		return fmt.Errorf("failed to set name: %d", err)
	}
	return nil
}

func (t *Tox) GetSelfStatus() string {
	size := C.tox_self_get_status_message_size(t.tox)
	if size == 0 {
		return ""
	}
	status := make([]byte, size)
	C.tox_self_get_status_message(t.tox, (*C.uint8_t)(unsafe.Pointer(&status[0])))
	return string(status)
}

func (t *Tox) SetSelfStatus(status string) error {
	cstatus := C.CString(status)
	defer C.free(unsafe.Pointer(cstatus))
	var err C.Tox_Err_Set_Info
	C.tox_self_set_status_message(t.tox, (*C.uint8_t)(unsafe.Pointer(cstatus)), C.size_t(len(status)), &err)
	if err != C.TOX_ERR_SET_INFO_OK {
		return fmt.Errorf("failed to set status: %d", err)
	}
	return nil
}

func (t *Tox) GetConnectionStatus() string {
	conn := C.tox_self_get_connection_status(t.tox)
	switch conn {
	case C.TOX_CONNECTION_NONE:
		return "offline"
	case C.TOX_CONNECTION_TCP:
		return "tcp"
	case C.TOX_CONNECTION_UDP:
		return "udp"
	default:
		return "unknown"
	}
}

func (t *Tox) GetFriendList() []uint32 {
	count := int(C.tox_self_get_friend_list_size(t.tox))
	if count == 0 {
		return nil
	}
	friends := make([]C.uint32_t, count)
	C.tox_self_get_friend_list(t.tox, &friends[0])
	result := make([]uint32, count)
	for i := 0; i < count; i++ {
		result[i] = uint32(friends[i])
	}
	return result
}

func (t *Tox) GetFriendName(friendID uint32) string {
	count := int(C.tox_friend_get_name_size(t.tox, C.uint32_t(friendID), nil))
	if count <= 0 {
		return ""
	}
	name := make([]byte, count)
	C.tox_friend_get_name(t.tox, C.uint32_t(friendID), (*C.uint8_t)(unsafe.Pointer(&name[0])), nil)
	return string(name)
}

func (t *Tox) AddFriend(publicKeyHex string) (uint32, error) {
	var pk [C.TOX_PUBLIC_KEY_SIZE]byte
	if len(publicKeyHex) < 64 {
		return 0, fmt.Errorf("public key too short")
	}
	for i := 0; i < C.TOX_PUBLIC_KEY_SIZE; i++ {
		_, err := fmt.Sscanf(publicKeyHex[i*2:i*2+2], "%02x", &pk[i])
		if err != nil {
			return 0, fmt.Errorf("invalid public key hex")
		}
	}
	
	var err C.Tox_Err_Friend_Add
	fn := C.tox_friend_add_norequest(t.tox, (*C.uint8_t)(unsafe.Pointer(&pk[0])), &err)
	if err != C.TOX_ERR_FRIEND_ADD_OK {
		return 0, fmt.Errorf("failed to add friend: %d", err)
	}
	return uint32(fn), nil
}

func (t *Tox) SendMessage(friendID uint32, message string) (uint32, error) {
	cmsg := C.CString(message)
	defer C.free(unsafe.Pointer(cmsg))
	
	var err C.Tox_Err_Friend_Send_Message
	msgID := C.tox_friend_send_message(t.tox, C.uint32_t(friendID), C.TOX_MESSAGE_TYPE_NORMAL, 
		(*C.uint8_t)(unsafe.Pointer(cmsg)), C.size_t(len(message)), &err)
	if err != C.TOX_ERR_FRIEND_SEND_MESSAGE_OK {
		return 0, fmt.Errorf("failed to send message: %d", err)
	}
	return uint32(msgID), nil
}

func (t *Tox) GetConferenceList() []uint32 {
	// toxcore C API doesn't provide a direct "get conference list" function
	// Need to track conferences in Go state
	return nil
}

func (t *Tox) NewConference() (uint32, error) {
	var err C.Tox_Err_Conference_New
	confNum := C.tox_conference_new(t.tox, &err)
	if err != C.TOX_ERR_CONFERENCE_NEW_OK {
		return 0, fmt.Errorf("failed to create conference: %d", err)
	}
	return uint32(confNum), nil
}

func (t *Tox) GetFriendConnectionStatus(friendID uint32) string {
	conn := C.tox_friend_get_connection_status(t.tox, C.uint32_t(friendID), nil)
	switch conn {
	case C.TOX_CONNECTION_NONE:
		return "offline"
	case C.TOX_CONNECTION_TCP:
		return "tcp"
	case C.TOX_CONNECTION_UDP:
		return "udp"
	default:
		return "unknown"
	}
}

func (t *Tox) GetFriendPublicKey(friendID uint32) string {
	var pk [C.TOX_PUBLIC_KEY_SIZE]byte
	C.tox_friend_get_public_key(t.tox, C.uint32_t(friendID), (*C.uint8_t)(unsafe.Pointer(&pk[0])), nil)
	hex := ""
	for _, b := range pk {
		hex += fmt.Sprintf("%02x", b)
	}
	return hex
}

func (t *Tox) Iterate() {
	C.tox_iterate(t.tox, nil)
}

func (t *Tox) IterationInterval() int {
	return int(C.tox_iteration_interval(t.tox))
}
