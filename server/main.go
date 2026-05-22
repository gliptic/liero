package main

import (
	"flag"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
)

func main() {
	port := flag.Int("port", 19533, "UDP listen port")
	flag.Parse()

	addr := &net.UDPAddr{Port: *port}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatalf("Failed to listen on :%d: %v", *port, err)
	}
	defer conn.Close()

	log.Printf("Signaling server listening on :%d", *port)
	if os.Getenv("TURN_SECRET") != "" {
		log.Printf("TURN credential generation enabled")
	}

	srv := NewServer(conn)

	// Graceful shutdown
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sig
		log.Println("Shutting down...")
		conn.Close()
	}()

	srv.Run()
}
