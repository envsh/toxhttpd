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

type SelfProvider interface {
	SelfGet() *SelfInfo
}

type MatrixServer struct {
	self   SelfProvider
	mu     sync.Mutex
	tokens map[string]string
}

func NewMatrixServer(s SelfProvider) *MatrixServer {
	return &MatrixServer{
		self:   s,
		tokens: make(map[string]string),
	}
}

func (ms *MatrixServer) RegisterMatrix(mux *http.ServeMux) {
	mux.HandleFunc("/_matrix/client/versions",
		corsMiddleware(loggingMiddleware(ms.handleVersions)))
	mux.HandleFunc("/_matrix/client/v3/login",
		corsMiddleware(loggingMiddleware(ms.handleLogin)))
	mux.HandleFunc("/_matrix/client/v3/account/whoami",
		corsMiddleware(loggingMiddleware(ms.handleWhoami)))
}

func (ms *MatrixServer) userID() string {
	return "@" + ms.self.SelfGet().Address + ":127.0.0.1"
}

func (ms *MatrixServer) newToken() string {
	b := make([]byte, 16)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func (ms *MatrixServer) handleVersions(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		writeJSON(w, map[string]string{
			"errcode": "M_UNRECOGNIZED",
			"error":   "Unrecognized endpoint",
		})
		return
	}
	writeJSON(w, map[string][]string{
		"versions": {"v1.11", "v1.12", "v1.13", "v1.14", "v1.15", "v1.16", "v1.17", "v1.18"},
	})
}

type loginRequest struct {
	Type     string `json:"type"`
	User     string `json:"user"`
	Password string `json:"password"`
}

func (ms *MatrixServer) handleLogin(w http.ResponseWriter, r *http.Request) {
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
				"base_url": "http://127.0.0.1:8181",
			},
		},
	})
}

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
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusUnauthorized)
		json.NewEncoder(w).Encode(map[string]string{
			"errcode": "M_MISSING_TOKEN",
			"error":   "Missing access token",
		})
		return
	}

	ms.mu.Lock()
	_, ok := ms.tokens[token]
	ms.mu.Unlock()

	if !ok {
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusUnauthorized)
		json.NewEncoder(w).Encode(map[string]string{
			"errcode": "M_UNKNOWN_TOKEN",
			"error":   "Invalid access token",
		})
		return
	}

	writeJSON(w, map[string]string{
		"user_id": ms.userID(),
	})
}

func extractBearer(r *http.Request) string {
	auth := r.Header.Get("Authorization")
	if strings.HasPrefix(auth, "Bearer ") {
		return strings.TrimPrefix(auth, "Bearer ")
	}
	return ""
}
