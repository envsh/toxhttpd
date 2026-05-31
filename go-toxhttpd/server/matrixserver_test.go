package server

import (
	"encoding/json"
	"net/http/httptest"
	"strings"
	"testing"
)

type mockSelf struct {
	addr string
	name string
}

func (m *mockSelf) SelfGet() *SelfInfo {
	return &SelfInfo{Address: m.addr, Name: m.name}
}

func newTestServer(addr string) *MatrixServer {
	return &MatrixServer{
		self:   &mockSelf{addr: addr},
		tokens: make(map[string]string),
	}
}

func doLogin(ms *MatrixServer, user, pass string) string {
	body := strings.NewReader(`{"user":"` + user + `","password":"` + pass + `"}`)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/login", body)
	r.Header.Set("Content-Type", "application/json")
	ms.handleLogin(w, r)
	var resp map[string]interface{}
	json.NewDecoder(w.Body).Decode(&resp)
	tok, _ := resp["access_token"].(string)
	return tok
}

const testAddr = "B1E2CA254FACA097F40491E3F96FEEDC5187D4BFC27D6F459BCF223ACF3CD9EDA428655B"

// ── extractBearer ──

func TestExtractBearer_Empty(t *testing.T) {
	r := httptest.NewRequest("GET", "/", nil)
	if got := extractBearer(r); got != "" {
		t.Fatalf("empty auth header: got %q, want \"\"", got)
	}
}

func TestExtractBearer_NoPrefix(t *testing.T) {
	r := httptest.NewRequest("GET", "/", nil)
	r.Header.Set("Authorization", "NotBearer abc123")
	if got := extractBearer(r); got != "" {
		t.Fatalf("no Bearer prefix: got %q, want \"\"", got)
	}
}

func TestExtractBearer_Valid(t *testing.T) {
	r := httptest.NewRequest("GET", "/", nil)
	r.Header.Set("Authorization", "Bearer mytoken123")
	if got := extractBearer(r); got != "mytoken123" {
		t.Fatalf("valid token: got %q, want %q", got, "mytoken123")
	}
}

// ── serveMatrix routing ──

func TestServeMatrix_UnknownPath(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/v3/ahahah", nil)
	ms.serveMatrix(w, r)
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}

func TestServeMatrix_Versions(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/versions", nil)
	ms.serveMatrix(w, r)
	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string][]string
	json.NewDecoder(w.Body).Decode(&resp)
	versions := resp["versions"]
	if len(versions) == 0 || versions[len(versions)-1] != "v1.18" {
		t.Fatalf("unexpected versions response")
	}
}

// ── handleVersions ──

func TestHandleVersions_GET(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/versions", nil)
	ms.handleVersions(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string][]string
	json.NewDecoder(w.Body).Decode(&resp)
	versions := resp["versions"]
	if len(versions) == 0 {
		t.Fatal("versions list empty")
	}
	if versions[len(versions)-1] != "v1.18" {
		t.Fatalf("last version: got %s, want v1.18", versions[len(versions)-1])
	}
}

func TestHandleVersions_NotGET(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/versions", nil)
	ms.handleVersions(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}

// ── handleLogin ──

func TestHandleLogin_Valid(t *testing.T) {
	ms := newTestServer(testAddr)
	body := strings.NewReader(`{"user":"alice","password":"secret"}`)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/login", body)
	r.Header.Set("Content-Type", "application/json")
	ms.handleLogin(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string]interface{}
	json.NewDecoder(w.Body).Decode(&resp)

	if _, ok := resp["access_token"]; !ok {
		t.Fatal("missing access_token")
	}
	if uid, _ := resp["user_id"].(string); uid != "@"+testAddr+":127.0.0.1" {
		t.Fatalf("user_id: got %s, want @%s:127.0.0.1", uid, testAddr)
	}
	wk, ok := resp["well_known"].(map[string]interface{})
	if !ok {
		t.Fatal("missing well_known")
	}
	mh, ok := wk["m.homeserver"].(map[string]interface{})
	if !ok {
		t.Fatal("missing well_known.m.homeserver")
	}
	if mh["base_url"] != "http://127.0.0.1:8181" {
		t.Fatalf("base_url: got %s, want http://127.0.0.1:8181", mh["base_url"])
	}
}

func TestHandleLogin_MissingUser(t *testing.T) {
	ms := newTestServer(testAddr)
	body := strings.NewReader(`{"password":"secret"}`)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/login", body)
	r.Header.Set("Content-Type", "application/json")
	ms.handleLogin(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_MISSING_PARAM" {
		t.Fatalf("errcode: got %s, want M_MISSING_PARAM", resp["errcode"])
	}
}

func TestHandleLogin_MissingPassword(t *testing.T) {
	ms := newTestServer(testAddr)
	body := strings.NewReader(`{"user":"alice"}`)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/login", body)
	r.Header.Set("Content-Type", "application/json")
	ms.handleLogin(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_MISSING_PARAM" {
		t.Fatalf("errcode: got %s, want M_MISSING_PARAM", resp["errcode"])
	}
}

func TestHandleLogin_BadJSON(t *testing.T) {
	ms := newTestServer(testAddr)
	body := strings.NewReader(`not json`)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/login", body)
	r.Header.Set("Content-Type", "application/json")
	ms.handleLogin(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_BAD_JSON" {
		t.Fatalf("errcode: got %s, want M_BAD_JSON", resp["errcode"])
	}
}

func TestHandleLogin_NotPOST(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/v3/login", nil)
	ms.handleLogin(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}

// ── handleLogout ──

func TestHandleLogout_Valid(t *testing.T) {
	ms := newTestServer(testAddr)
	token := doLogin(ms, "alice", "x")

	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/logout", nil)
	r.Header.Set("Authorization", "Bearer "+token)
	ms.handleLogout(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}

	// token should be gone
	ms.mu.Lock()
	_, ok := ms.tokens[token]
	ms.mu.Unlock()
	if ok {
		t.Fatal("token should be deleted after logout")
	}
}

func TestHandleLogout_NoToken(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/logout", nil)
	ms.handleLogout(w, r)

	if w.Code != 401 {
		t.Fatalf("status: got %d, want 401", w.Code)
	}
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_MISSING_TOKEN" {
		t.Fatalf("errcode: got %s, want M_MISSING_TOKEN", resp["errcode"])
	}
}

func TestHandleLogout_BadToken(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/logout", nil)
	r.Header.Set("Authorization", "Bearer invalidtoken")
	ms.handleLogout(w, r)

	if w.Code != 401 {
		t.Fatalf("status: got %d, want 401", w.Code)
	}
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNKNOWN_TOKEN" {
		t.Fatalf("errcode: got %s, want M_UNKNOWN_TOKEN", resp["errcode"])
	}
}

func TestHandleLogout_NotPOST(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/v3/logout", nil)
	ms.handleLogout(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}

// ── handleWhoami ──

func TestHandleWhoami_Valid(t *testing.T) {
	ms := newTestServer(testAddr)
	token := doLogin(ms, "alice", "x")

	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/v3/account/whoami", nil)
	r.Header.Set("Authorization", "Bearer "+token)
	ms.handleWhoami(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	want := "@" + testAddr + ":127.0.0.1"
	if resp["user_id"] != want {
		t.Fatalf("user_id: got %s, want %s", resp["user_id"], want)
	}
}

func TestHandleWhoami_NoToken(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/v3/account/whoami", nil)
	ms.handleWhoami(w, r)

	if w.Code != 401 {
		t.Fatalf("status: got %d, want 401", w.Code)
	}
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_MISSING_TOKEN" {
		t.Fatalf("errcode: got %s, want M_MISSING_TOKEN", resp["errcode"])
	}
}

func TestHandleWhoami_BadToken(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/_matrix/client/v3/account/whoami", nil)
	r.Header.Set("Authorization", "Bearer invalidtoken")
	ms.handleWhoami(w, r)

	if w.Code != 401 {
		t.Fatalf("status: got %d, want 401", w.Code)
	}
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNKNOWN_TOKEN" {
		t.Fatalf("errcode: got %s, want M_UNKNOWN_TOKEN", resp["errcode"])
	}
}

func TestHandleWhoami_NotGET(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/account/whoami", nil)
	ms.handleWhoami(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}

// ── handleProfile ──

func TestHandleProfile_Full(t *testing.T) {
	ms := newTestServer(testAddr)
	ms.self.(*mockSelf).name = "ToxUser"

	p := "/_matrix/client/v3/profile/@" + testAddr + ":127.0.0.1"
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", p, nil)
	ms.handleProfile(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string]interface{}
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["displayname"] != "ToxUser" {
		t.Fatalf("displayname: got %v, want ToxUser", resp["displayname"])
	}
	if resp["avatar_url"] != nil {
		t.Fatalf("avatar_url: got %v, want nil", resp["avatar_url"])
	}
}

func TestHandleProfile_Displayname(t *testing.T) {
	ms := newTestServer(testAddr)
	ms.self.(*mockSelf).name = "ToxUser"

	p := "/_matrix/client/v3/profile/@" + testAddr + ":127.0.0.1/displayname"
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", p, nil)
	ms.handleProfile(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["displayname"] != "ToxUser" {
		t.Fatalf("displayname: got %s, want ToxUser", resp["displayname"])
	}
}

func TestHandleProfile_AvatarUrl(t *testing.T) {
	ms := newTestServer(testAddr)

	p := "/_matrix/client/v3/profile/@" + testAddr + ":127.0.0.1/avatar_url"
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", p, nil)
	ms.handleProfile(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string]interface{}
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["avatar_url"] != nil {
		t.Fatalf("avatar_url: got %v, want nil", resp["avatar_url"])
	}
}

func TestHandleProfile_InvalidUserID(t *testing.T) {
	ms := newTestServer(testAddr)
	tests := []string{
		"/_matrix/client/v3/profile/@short:127.0.0.1",
		"/_matrix/client/v3/profile/@" + testAddr + ":wronghost",
		"/_matrix/client/v3/profile/" + testAddr + ":127.0.0.1", // missing @
	}
	for _, p := range tests {
		w := httptest.NewRecorder()
		r := httptest.NewRequest("GET", p, nil)
		ms.handleProfile(w, r)
		if w.Code != 400 {
			t.Fatalf("path=%q: status got %d, want 400", p, w.Code)
		}
		var resp map[string]string
		json.NewDecoder(w.Body).Decode(&resp)
		if resp["errcode"] != "M_INVALID_PARAM" {
			t.Fatalf("path=%q: errcode got %s, want M_INVALID_PARAM", p, resp["errcode"])
		}
	}
}

func TestHandleProfile_NotGET(t *testing.T) {
	ms := newTestServer(testAddr)
	p := "/_matrix/client/v3/profile/@" + testAddr + ":127.0.0.1"
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", p, nil)
	ms.handleProfile(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}

// ── handleWellKnown ──

func TestHandleWellKnown_GET(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("GET", "/.well-known/matrix/client", nil)
	ms.handleWellKnown(w, r)

	if w.Code != 200 {
		t.Fatalf("status: got %d, want 200", w.Code)
	}
	var resp map[string]interface{}
	json.NewDecoder(w.Body).Decode(&resp)
	mh, ok := resp["m.homeserver"].(map[string]interface{})
	if !ok {
		t.Fatal("missing m.homeserver")
	}
	if mh["base_url"] != "http://127.0.0.1:8181" {
		t.Fatalf("base_url: got %s, want http://127.0.0.1:8181", mh["base_url"])
	}
}

func TestHandleWellKnown_NotGET(t *testing.T) {
	ms := newTestServer(testAddr)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/.well-known/matrix/client", nil)
	ms.handleWellKnown(w, r)

	var resp map[string]string
	json.NewDecoder(w.Body).Decode(&resp)
	if resp["errcode"] != "M_UNRECOGNIZED" {
		t.Fatalf("errcode: got %s, want M_UNRECOGNIZED", resp["errcode"])
	}
}
