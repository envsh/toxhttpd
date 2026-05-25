package main

import (
	"log"
	"os"
	"os/signal"
	"syscall"

	"github.com/envsh/toxhttpd/server"
)

func main() {
	cfg := server.ParseFlags()
	srv, err := server.New(cfg)
	if err != nil {
		log.Fatalf("Failed to create server: %v", err)
	}

	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sig
		srv.Shutdown()
	}()

	if err := srv.Start(); err != nil {
		log.Fatal(err)
	}
}
