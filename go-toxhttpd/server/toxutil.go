package server

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"unsafe"

	tox "github.com/TokTok/go-toxcore-c"
)

type toxemu struct {
	opts unsafe.Pointer
	ctox unsafe.Pointer
}

func getInnerPtr(t *tox.Tox) unsafe.Pointer {
	x := (*toxemu)(unsafe.Pointer(t))
	return x.ctox
}

func logToxStatus(t *tox.Tox) {
	addr := t.SelfGetAddress()
	name := t.SelfGetName()
	connStatus := t.SelfGetConnectionStatus()
	var connStr string
	switch connStatus {
	case tox.CONNECTION_NONE:
		connStr = "OFFLINE"
	case tox.CONNECTION_TCP:
		connStr = "TCP"
	case tox.CONNECTION_UDP:
		connStr = "UDP"
	default:
		connStr = "UNKNOWN"
	}
	friends := t.SelfGetFriendList()
	log.Printf("[TOX] Status: name=%s, addr=%s, connection=%s (%d), friends=%d",
		name, addr[:16]+"...", connStr, connStatus, len(friends))
}

func saveToxData(t *tox.Tox, path string) {
	os.MkdirAll("data", 0700)
	err := t.WriteSavedata(path)
	if err != nil {
		log.Printf("[TOX] Failed to save data to %s: %v", path, err)
	} else {
		log.Printf("[TOX] Saved data to %s", path)
	}
}

func setLogLevel(level string) {
	var toxLevel int
	switch level {
	case "trace":
		toxLevel = tox.LOG_LEVEL_TRACE
	case "debug":
		toxLevel = tox.LOG_LEVEL_DEBUG
	case "info":
		toxLevel = tox.LOG_LEVEL_INFO
	case "warn":
		toxLevel = tox.LOG_LEVEL_WARNING
	case "error":
		toxLevel = tox.LOG_LEVEL_ERROR
	default:
		toxLevel = tox.LOG_LEVEL_INFO
	}
	tox.SetLogLevel(toxLevel)
	log.Printf("[MAIN] Log level set to: %s (toxcore level: %d)", level, toxLevel)
}

func detectWebRoot() (string, error) {
	if exe, err := os.Executable(); err == nil {
		exeDir := filepath.Dir(exe)
		if fi, err := os.Stat(filepath.Join(exeDir, "web")); err == nil && fi.IsDir() {
			return exeDir, nil
		}
	}
	if cwd, err := os.Getwd(); err == nil {
		if fi, err := os.Stat(filepath.Join(cwd, "web")); err == nil && fi.IsDir() {
			return cwd, nil
		}
	}
	return "", fmt.Errorf("web/ directory not found: checked exe dir and current dir")
}

func safeTruncate(s string, maxLen int) string {
	if len(s) <= maxLen {
		return s
	}
	return s[:maxLen]
}

func statusToStr(s int) string {
	switch s {
	case 1:
		return "tcp"
	case 2:
		return "udp"
	default:
		return "none"
	}
}

func groupStatusToStr(s int) string {
	if s == 1 {
		return "online"
	}
	return "none"
}

func roleToStr(role int) string {
	switch role {
	case 0:
		return "founder"
	case 1:
		return "moderator"
	case 2:
		return "member"
	case 3:
		return "observer"
	default:
		return "unknown"
	}
}
