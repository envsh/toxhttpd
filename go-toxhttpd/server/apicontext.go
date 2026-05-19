package server

import (
	"database/sql"
	"sync"

	tox "github.com/TokTok/go-toxcore-c"
	"github.com/envsh/toxera/toxpriv"
)

type ApiContext struct {
	Tox                  *tox.Tox
	Toxp                 *toxpriv.Tox
	DB                   *sql.DB
	EventQueue           *EventQueue
	SelfConnectionStatus string
	FriendStatuses       map[uint32]string
	FriendUserStatuses   map[uint32]int
	ConferenceConnected  map[uint32]bool
	Mu                   sync.RWMutex
}

func NewApiContext(t *tox.Tox, toxp *toxpriv.Tox, db *sql.DB) *ApiContext {
	return &ApiContext{
		Tox:                 t,
		Toxp:                toxp,
		DB:                  db,
		EventQueue:          NewEventQueue(),
		SelfConnectionStatus: "offline",
		FriendStatuses:      make(map[uint32]string),
		FriendUserStatuses:  make(map[uint32]int),
		ConferenceConnected: make(map[uint32]bool),
	}
}
