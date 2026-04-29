package log

import (
	"fmt"
	"os"
	"runtime"
	"strings"
	"sync"
	"time"
)

type LogLevel int

const (
	LOG_DEBUG LogLevel = iota
	LOG_INFO
	LOG_WARN
	LOG_ERROR
)

var (
	level   LogLevel = LOG_INFO
	output  *os.File = os.Stdout
	mu      sync.Mutex
)

func SetLevel(l LogLevel) {
	mu.Lock()
	defer mu.Unlock()
	level = l
}

func SetOutput(f *os.File) {
	mu.Lock()
	defer mu.Unlock()
	output = f
}

func logf(l LogLevel, file string, line int, fmtStr string, args ...interface{}) {
	if l < level {
		return
	}
	
	mu.Lock()
	defer mu.Unlock()
	
	// Get level prefix
	var prefix string
	switch l {
	case LOG_DEBUG:
		prefix = "DEBUG"
	case LOG_INFO:
		prefix = "INFO"
	case LOG_WARN:
		prefix = "WARN"
	case LOG_ERROR:
		prefix = "ERROR"
	}
	
	// Shorten file path
	shortFile := file
	if idx := strings.LastIndex(file, "/"); idx >= 0 {
		shortFile = file[idx+1:]
	}
	
	// Format: [TIME] LEVEL file:line message
	ts := time.Now().Format("2006-01-02 15:04:05")
	msg := fmt.Sprintf(fmtStr, args...)
	fmt.Fprintf(output, "[%s] %s %s:%d %s\n", ts, prefix, shortFile, line, msg)
}

func Debug(fmtStr string, args ...interface{}) {
	_, file, line, _ := runtime.Caller(1)
	logf(LOG_DEBUG, file, line, fmtStr, args...)
}

func Info(fmtStr string, args ...interface{}) {
	_, file, line, _ := runtime.Caller(1)
	logf(LOG_INFO, file, line, fmtStr, args...)
}

func Warn(fmtStr string, args ...interface{}) {
	_, file, line, _ := runtime.Caller(1)
	logf(LOG_WARN, file, line, fmtStr, args...)
}

func Error(fmtStr string, args ...interface{}) {
	_, file, line, _ := runtime.Caller(1)
	logf(LOG_ERROR, file, line, fmtStr, args...)
}
