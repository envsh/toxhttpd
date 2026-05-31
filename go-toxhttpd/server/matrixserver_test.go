package server

import (
	"encoding/json"
	"net/http/httptest"
	"strings"
	"testing"
)

type mockSelf struct{ addr string }

func (m *mockSelf) SelfGet() *SelfInfo {
	return &SelfInfo{Address: m.addr}
}

func newTestServer(addr string) *MatrixServer {
	return &MatrixServer{
		self:   &mockSelf{addr: addr},
		tokens: make(map[string]string),
	}
}

const testAddr = "0A1B2C3D4E5F60718293A4B5C6D7E8F90A1B2C3D4E5F60718293A4B5C6D7E8F"

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

func TestHandleWhoami_Valid(t *testing.T) {
	ms := newTestServer(testAddr)
	body := strings.NewReader(`{"user":"alice","password":"x"}`)
	w := httptest.NewRecorder()
	r := httptest.NewRequest("POST", "/_matrix/client/v3/login", body)
	r.Header.Set("Content-Type", "application/json")
	ms.handleLogin(w, r)
	var loginResp map[string]interface{}
	json.NewDecoder(w.Body).Decode(&loginResp)
	token := loginResp["access_token"].(string)

	w2 := httptest.NewRecorder()
	r2 := httptest.NewRequest("GET", "/_matrix/client/v3/account/whoami", nil)
	r2.Header.Set("Authorization", "Bearer "+token)
	ms.handleWhoami(w2, r2)

	if w2.Code != 200 {
		t.Fatalf("status: got %d, want 200", w2.Code)
	}
	var whoamiResp map[string]string
	json.NewDecoder(w2.Body).Decode(&whoamiResp)
	want := "@" + testAddr + ":127.0.0.1"
	if whoamiResp["user_id"] != want {
		t.Fatalf("user_id: got %s, want %s", whoamiResp["user_id"], want)
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
