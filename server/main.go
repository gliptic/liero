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
	relayPortBase := flag.Int("relay-port-base", 19600, "Base port for UDP relay allocations")
	relayPortCount := flag.Int("relay-port-count", 100, "Number of ports available for relay")
	flag.Parse()

	addr := &net.UDPAddr{Port: *port}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatalf("Failed to listen on :%d: %v", *port, err)
	}
	defer conn.Close()

	log.Printf("Signaling server listening on :%d (relay ports %d-%d)",
		*port, *relayPortBase, *relayPortBase+*relayPortCount-1)

	srv := NewServer(conn, *relayPortBase, *relayPortCount)

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
