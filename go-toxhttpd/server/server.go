package server

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"

	tox "github.com/TokTok/go-toxcore-c"
	"github.com/envsh/toxera/toxpriv"
)

type Server struct {
	ApiContext
	webRoot    string
	config     Config
	rebscnter  int
	shutdownCh chan struct{}
	httpServer *http.Server
}

func New(cfg Config) (*Server, error) {
	saveDataPath := "data/savedata.bin"
	var saveData []byte
	if data, err := os.ReadFile(saveDataPath); err == nil && len(data) > 100 {
		saveData = data
		log.Printf("[TOX] Loaded save data from %s (%d bytes)", saveDataPath, len(data))
	} else {
		log.Printf("[TOX] No save data found, creating new identity")
	}

	opts := tox.NewToxOptions()
	opts.Udp_enabled = cfg.UDPEnabled
	opts.GroupsPersistence = true
	if saveData != nil {
		opts.Savedata_type = tox.SAVEDATA_TYPE_TOX_SAVE
		opts.Savedata_data = saveData
	}
	t := tox.NewTox(opts)
	if t == nil {
		return nil, fmt.Errorf("failed to create tox instance")
	}

	db, err := initMsgHistDB("data/msghist.db")
	if err != nil {
		return nil, fmt.Errorf("failed to init msghist db: %w", err)
	}

	webRoot := cfg.WebRoot
	if webRoot == "" {
		wr, err := detectWebRoot()
		if err != nil {
			return nil, fmt.Errorf("web root detection failed: %w", err)
		}
		webRoot = wr
	}

	ctx := NewApiContext(t, toxpriv.NewTox(getInnerPtr(t)), db)

	server := &Server{
		ApiContext: *ctx,
		webRoot:    webRoot,
		config:     cfg,
		shutdownCh: make(chan struct{}),
	}

	// Set up callbacks via Midapi
	m := &Midapi{ctx: ctx}
	m.SetupCallbacks()

	friends := t.SelfGetFriendList()
	if len(friends) == 0 {
		log.Println("No friends found, adding test friend...")
		log.Println("Note: Add friends via /api/friends POST with public_key")
	}
	addr := t.SelfGetAddress()
	log.Printf("Tox ID: %s", addr)
	log.Println("Server ready with saved tox identity")

	log.Println("[TOX] Starting bootstrap to 3 nodes...")
	bootstrapAll(t)
	log.Println("[TOX] Bootstrap completed")
	logToxStatus(t)

	log.Printf("[TOX] UDP enabled: %v", cfg.UDPEnabled)

	return server, nil
}

func (s *Server) Start() error {
	m := NewMidapi(&s.ApiContext)
	h := NewRestapi(m, s.webRoot)

	mux := http.NewServeMux()
	h.Register(mux)

	// Start tox iteration in background
	ctx, cancel := context.WithCancel(context.Background())
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			select {
			case <-ctx.Done():
				return
			default:
				s.Tox.Iterate()
				s.checkRebootstrap()
				time.Sleep(time.Millisecond * time.Duration(s.Tox.IterationInterval()))
			}
		}
	}()

	s.httpServer = &http.Server{Addr: ":" + s.config.Port, Handler: mux}

	httpErr := make(chan error, 1)
	go func() {
		log.Printf("Server starting on :%s", s.config.Port)
		if err := s.httpServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			httpErr <- err
		}
	}()

	select {
	case err := <-httpErr:
		cancel()
		wg.Wait()
		return err
	case <-s.shutdownCh:
		log.Println("Shutting down...")
		cancel()
		wg.Wait()
		saveToxData(s.Tox, "data/savedata.bin")
		shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 5*time.Second)
		defer shutdownCancel()
		s.httpServer.Shutdown(shutdownCtx)
		s.Tox.Kill()
		log.Println("Server stopped.")
		return nil
	}
}

func (s *Server) Shutdown() {
	close(s.shutdownCh)
}
