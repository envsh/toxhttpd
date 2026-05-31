package server

import (
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"log"
	"net/http"
	"strings"
	"sync"
)

const matrixHost = "127.0.0.1:8181"

type SelfProvider interface {
	SelfGet() *SelfInfo
}

type MatrixServer struct {
	self     SelfProvider
	midapi   *Midapi
	mu       sync.Mutex
	tokens   map[string]string
	v5mu     sync.Mutex
	v5after  uint64
	v5bump   int64
}

func NewMatrixServer(s SelfProvider, m *Midapi) *MatrixServer {
	return &MatrixServer{
		self:     s,
		midapi:   m,
		tokens:   make(map[string]string),
	}
}

func (ms *MatrixServer) RegisterMatrix(mux *http.ServeMux) {
	mux.HandleFunc("/_matrix/", corsMiddleware(loggingMiddleware(ms.serveMatrix)))
	mux.HandleFunc("/.well-known/matrix/client", corsMiddleware(loggingMiddleware(ms.handleWellKnown)))
}

func (ms *MatrixServer) serveMatrix(w http.ResponseWriter, r *http.Request) {
	path := r.URL.Path
	switch {
	case path == "/_matrix/client/versions":
		ms.handleVersions(w, r)
	case path == "/_matrix/client/v3/login":
		ms.handleLogin(w, r)
	case path == "/_matrix/client/v3/logout":
		ms.handleLogout(w, r)
	case path == "/_matrix/client/v3/logout/all":
		ms.handleLogout(w, r)
	case path == "/_matrix/client/v3/account/whoami":
		ms.handleWhoami(w, r)
	case strings.HasPrefix(path, "/_matrix/client/v3/profile/"):
		ms.handleProfile(w, r)
	case path == "/_matrix/client/v3/capabilities":
		writeJSON(w, map[string]interface{}{
			"capabilities": map[string]interface{}{
				"m.room_versions": map[string]interface{}{
					"default": "10",
					"available": map[string]string{
						"1": "stable", "2": "stable", "3": "stable",
						"4": "stable", "5": "stable", "6": "stable",
						"7": "stable", "8": "stable", "9": "stable",
						"10": "stable",
					},
				},
				"m.change_password": map[string]bool{"enabled": false},
			},
		})
	case path == "/_matrix/client/v5/sync":
		ms.handleV5Sync(w, r)
	case strings.HasPrefix(path, "/_matrix/client/v3/pushrules"):
		suffix := strings.TrimPrefix(path, "/_matrix/client/v3/pushrules")
		switch {
		case suffix == "" || suffix == "/":
			writeJSON(w, map[string]interface{}{
				"global": map[string][]interface{}{
					"content": {}, "override": {},
					"room": {}, "sender": {}, "underride": {},
				},
			})
		case strings.Count(suffix, "/") == 2:
			writeJSON(w, []interface{}{})
		default:
			writeJSON(w, map[string]interface{}{
				"rule_id": "default",
				"actions": []string{"notify"},
				"default": true,
				"enabled": true,
			})
		}
	case path == "/_matrix/client/v3/sync":
		ms.handleV3Sync(w, r)
	case path == "/_matrix/client/v3/keys/query":
		var req struct {
			DeviceKeys map[string][]string `json:"device_keys"`
		}
		json.NewDecoder(r.Body).Decode(&req)
		dk := make(map[string]interface{}, len(req.DeviceKeys))
		for uid := range req.DeviceKeys {
			dk[uid] = map[string]interface{}{}
		}
		writeJSON(w, map[string]interface{}{
			"device_keys": dk,
			"failures":    map[string]interface{}{},
		})
	case path == "/_matrix/client/v3/keys/upload":
		writeJSON(w, map[string]interface{}{
			"one_time_key_counts": map[string]int{"signed_curve25519": 50},
		})
	case path == "/_matrix/client/v3/room_keys/version":
		writeJSON(w, map[string]interface{}{
			"version":   nil,
			"algorithm": nil,
		})
	default:
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
		})
	}
}

func (ms *MatrixServer) userID() string {
	return "@" + ms.self.SelfGet().Address + ":" + matrixHost
}

func (ms *MatrixServer) newToken() string {
	b := make([]byte, 16)
	rand.Read(b)
	return hex.EncodeToString(b)
}

// ── GET /_matrix/client/versions ──

func (ms *MatrixServer) handleVersions(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
		})
		return
	}
	writeJSON(w, map[string][]string{
		"versions": {"v1.11", "v1.12", "v1.13", "v1.14", "v1.15", "v1.16", "v1.17", "v1.18", "v5"},
	})
}

// ── POST /_matrix/client/v3/login ──

type loginRequest struct {
	Type       string           `json:"type"`
	User       string           `json:"user"`
	Password   string           `json:"password"`
	Identifier *loginIdentifier `json:"identifier,omitempty"`
}

type loginIdentifier struct {
	Type string `json:"type"`
	User string `json:"user"`
}

func (ms *MatrixServer) handleLogin(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodGet {
		writeJSON(w, map[string]interface{}{
			"flows": []map[string]string{
				{"type": "m.login.password"},
			},
		})
		return
	}
	if r.Method != http.MethodPost {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
		})
		return
	}

	var req loginRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, map[string]string{
			"errcode": "M_BAD_JSON",
			"error":   "Invalid JSON",
		})
		return
	}
	if req.User == "" && req.Identifier != nil {
		req.User = req.Identifier.User
	}
	if req.User == "" || req.Password == "" {
		writeJSON(w, map[string]string{
			"errcode": "M_MISSING_PARAM",
			"error":   "Missing user or password",
		})
		return
	}

	token := ms.newToken()
	uid := ms.userID()

	ms.mu.Lock()
	ms.tokens[token] = req.User
	ms.mu.Unlock()

	log.Printf("[MATRIX] login: user=%q token=%s.. user_id=%s", req.User, token[:8], uid)

	writeJSON(w, map[string]interface{}{
		"access_token": token,
		"user_id":      uid,
		"device_id":    "MATRIX",
		"well_known": map[string]interface{}{
			"m.homeserver": map[string]string{
				"base_url": "http://" + matrixHost,
			},
		},
	})
}

// ── POST /_matrix/client/v3/logout ──

func (ms *MatrixServer) handleLogout(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
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
	delete(ms.tokens, token)
	ms.mu.Unlock()

	if !ok {
		writeMatrixUnauthorized(w, "M_UNKNOWN_TOKEN", "Invalid access token")
		return
	}

	writeJSON(w, map[string]interface{}{})
}

// ── GET /_matrix/client/v3/account/whoami ──

func (ms *MatrixServer) handleWhoami(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
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

	writeJSON(w, map[string]string{
		"user_id": ms.userID(),
	})
}

// ── GET /_matrix/client/v3/profile/<user_id> ──

func (ms *MatrixServer) handleProfile(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
		})
		return
	}

	path := r.URL.Path
	userPart := strings.TrimPrefix(path, "/_matrix/client/v3/profile/")

	subpath := ""
	switch {
	case strings.HasSuffix(userPart, "/displayname"):
		subpath = "displayname"
		userPart = strings.TrimSuffix(userPart, "/displayname")
	case strings.HasSuffix(userPart, "/avatar_url"):
		subpath = "avatar_url"
		userPart = strings.TrimSuffix(userPart, "/avatar_url")
	}

	if !strings.HasPrefix(userPart, "@") || !strings.HasSuffix(userPart, ":"+matrixHost) {
		writeProfileError(w)
		return
	}

	localpart := userPart[1 : len(userPart)-len(":"+matrixHost)]
	if !isHex72(localpart) {
		writeProfileError(w)
		return
	}

	info := ms.self.SelfGet()

	switch subpath {
	case "displayname":
		writeJSON(w, map[string]string{"displayname": info.Name})
	case "avatar_url":
		writeJSON(w, map[string]interface{}{"avatar_url": nil})
	default:
		writeJSON(w, map[string]interface{}{
			"displayname": info.Name,
			"avatar_url":  nil,
		})
	}
}

// ── GET /.well-known/matrix/client ──

func (ms *MatrixServer) handleWellKnown(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
		})
		return
	}
	writeJSON(w, map[string]interface{}{
		"m.homeserver": map[string]string{
			"base_url": "http://" + matrixHost,
		},
	})
}

func writeProfileError(w http.ResponseWriter) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusBadRequest)
	json.NewEncoder(w).Encode(map[string]string{
		"errcode": "M_INVALID_PARAM",
		"error":   "Invalid user ID",
	})
}

// ── helpers ──

func isHex72(s string) bool {
	if len(s) != 72 {
		return false
	}
	for _, c := range s {
		if !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
			return false
		}
	}
	return true
}

func writeMatrixUnauthorized(w http.ResponseWriter, errcode, msg string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusUnauthorized)
	json.NewEncoder(w).Encode(map[string]string{"errcode": errcode, "error": msg})
}

func extractBearer(r *http.Request) string {
	auth := r.Header.Get("Authorization")
	if strings.HasPrefix(auth, "Bearer ") {
		return strings.TrimPrefix(auth, "Bearer ")
	}
	return ""
}
