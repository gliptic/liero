package main

import (
	"crypto/rand"
	"encoding/binary"
	"log"
	"net"
	"sync"
	"time"
)

const (
	roomTTL       = 60 * time.Second
	cleanupPeriod = 10 * time.Second
	maxRooms      = 1000
)

// PeerAddr holds one discovered address for a peer.
type PeerAddr struct {
	Type byte   // AddrIPv4 or AddrIPv6
	IP   net.IP // 4 or 16 bytes
	Port uint16
}

// Peer represents one connected game client.
type Peer struct {
	Addr      *net.UDPAddr // UDP source address for sending replies
	Addresses []PeerAddr   // STUN-discovered external addresses
	PunchOK   bool
	PunchFail bool
}

// Room holds two peers trying to connect.
type Room struct {
	Code      [RoomCodeLen]byte
	Host      *Peer
	Client    *Peer
	CreatedAt time.Time
	LastSeen  time.Time
	RelayPort int // 0 = not allocated
}

type Server struct {
	conn          *net.UDPConn
	mu            sync.Mutex
	rooms         map[string]*Room // code string → room
	relayPortBase int
	relayPortMax  int
	relayPorts    map[int]bool // allocated relay ports
}

func NewServer(conn *net.UDPConn, relayPortBase, relayPortCount int) *Server {
	return &Server{
		conn:          conn,
		rooms:         make(map[string]*Room),
		relayPortBase: relayPortBase,
		relayPortMax:  relayPortBase + relayPortCount,
		relayPorts:    make(map[int]bool),
	}
}

func (s *Server) Run() {
	go s.cleanupLoop()

	buf := make([]byte, 512)
	for {
		n, addr, err := s.conn.ReadFromUDP(buf)
		if err != nil {
			log.Printf("Read error: %v", err)
			return
		}
		if n < 1 {
			continue
		}
		s.handlePacket(buf[:n], addr)
	}
}

func (s *Server) handlePacket(data []byte, from *net.UDPAddr) {
	msgType := data[0]
	log.Printf("Recv msg=0x%02x (%d bytes) from %s", msgType, len(data), from)

	switch msgType {
	case MsgCreateRoom:
		s.handleCreateRoom(from)
	case MsgJoinRoom:
		if len(data) < 1+RoomCodeLen {
			s.sendError(from, ErrInvalidMsg, "too short")
			return
		}
		code := string(data[1 : 1+RoomCodeLen])
		s.handleJoinRoom(code, from)
	case MsgReportAddr:
		if len(data) < 1+RoomCodeLen+1+2+4 {
			s.sendError(from, ErrInvalidMsg, "too short")
			return
		}
		code := string(data[1 : 1+RoomCodeLen])
		addrType := data[1+RoomCodeLen]
		port := binary.BigEndian.Uint16(data[1+RoomCodeLen+1:])
		var ipLen int
		if addrType == AddrIPv4 {
			ipLen = 4
		} else if addrType == AddrIPv6 {
			ipLen = 16
		} else {
			s.sendError(from, ErrInvalidMsg, "bad addr type")
			return
		}
		if len(data) < 1+RoomCodeLen+1+2+ipLen {
			s.sendError(from, ErrInvalidMsg, "too short for IP")
			return
		}
		ip := make(net.IP, ipLen)
		copy(ip, data[1+RoomCodeLen+3:1+RoomCodeLen+3+ipLen])
		s.handleReportAddr(code, from, PeerAddr{Type: addrType, IP: ip, Port: port})
	case MsgPunchOK:
		if len(data) < 1+RoomCodeLen {
			return
		}
		code := string(data[1 : 1+RoomCodeLen])
		s.handlePunchResult(code, from, true)
	case MsgPunchFail:
		if len(data) < 1+RoomCodeLen {
			return
		}
		code := string(data[1 : 1+RoomCodeLen])
		s.handlePunchResult(code, from, false)
	case MsgKeepalive:
		if len(data) < 1+RoomCodeLen {
			return
		}
		code := string(data[1 : 1+RoomCodeLen])
		s.mu.Lock()
		if room, ok := s.rooms[code]; ok {
			room.LastSeen = time.Now()
		}
		s.mu.Unlock()
	default:
		s.sendError(from, ErrInvalidMsg, "unknown type")
	}
}

func (s *Server) handleCreateRoom(from *net.UDPAddr) {
	s.mu.Lock()
	defer s.mu.Unlock()

	if len(s.rooms) >= maxRooms {
		s.sendError(from, ErrRoomFull, "server full")
		return
	}

	code := s.generateRoomCode()
	now := time.Now()
	room := &Room{
		Host:      &Peer{Addr: from},
		CreatedAt: now,
		LastSeen:  now,
	}
	copy(room.Code[:], code)
	s.rooms[code] = room

	log.Printf("Room %s created by %s", code, from)

	// Send RoomCreated
	resp := make([]byte, 1+RoomCodeLen)
	resp[0] = MsgRoomCreated
	copy(resp[1:], code)
	s.conn.WriteToUDP(resp, from)
}

func (s *Server) handleJoinRoom(code string, from *net.UDPAddr) {
	s.mu.Lock()
	defer s.mu.Unlock()

	room, ok := s.rooms[code]
	if !ok {
		s.sendError(from, ErrRoomNotFound, "no such room")
		return
	}
	if room.Client != nil {
		s.sendError(from, ErrRoomFull, "room full")
		return
	}

	room.Client = &Peer{Addr: from}
	room.LastSeen = time.Now()

	log.Printf("Room %s: client joined from %s", code, from)

	// Notify host that peer joined
	msg := make([]byte, 1+RoomCodeLen)
	msg[0] = MsgPeerJoined
	copy(msg[1:], code)
	s.conn.WriteToUDP(msg, room.Host.Addr)

	// Send any addresses the host already reported to the new client
	for _, a := range room.Host.Addresses {
		s.sendPeerAddr(from, code, a)
	}

	// If host has addresses, tell both to start punching
	if len(room.Host.Addresses) > 0 {
		s.sendStartPunch(room, code)
	}
}

func (s *Server) handleReportAddr(code string, from *net.UDPAddr, addr PeerAddr) {
	s.mu.Lock()
	defer s.mu.Unlock()

	room, ok := s.rooms[code]
	if !ok {
		log.Printf("Room %s: ReportAddr from %s but room not found", code, from)
		s.sendError(from, ErrRoomNotFound, "no such room")
		return
	}
	room.LastSeen = time.Now()

	peer := s.findPeer(room, from)
	if peer == nil {
		log.Printf("Room %s: ReportAddr from %s but not a known peer (host=%s, client=%v)",
			code, from, room.Host.Addr, room.Client)
		if room.Client != nil {
			log.Printf("  client addr: %s", room.Client.Addr)
		}
		s.sendError(from, ErrInvalidMsg, "not in room")
		return
	}

	peer.Addresses = append(peer.Addresses, addr)

	// Forward this address to the other peer
	other := s.otherPeer(room, peer)
	if other != nil {
		s.sendPeerAddr(other.Addr, code, addr)

		// If both have reported addresses, signal start punch
		if len(room.Host.Addresses) > 0 && room.Client != nil && len(room.Client.Addresses) > 0 {
			s.sendStartPunch(room, code)
		}
	}
}

func (s *Server) handlePunchResult(code string, from *net.UDPAddr, ok bool) {
	s.mu.Lock()
	defer s.mu.Unlock()

	room, exists := s.rooms[code]
	if !exists {
		log.Printf("Room %s: PunchResult from %s but room not found", code, from)
		return
	}

	peer := s.findPeer(room, from)
	if peer == nil {
		log.Printf("Room %s: PunchResult from %s but not a known peer (host=%s, client=%v)",
			code, from, room.Host.Addr, room.Client)
		if room.Client != nil {
			log.Printf("  client addr: %s", room.Client.Addr)
		}
		return
	}

	if ok {
		peer.PunchOK = true
		log.Printf("Room %s: punch OK from %s", code, from)
		// Both succeeded — room served its purpose, will be cleaned up
	} else {
		peer.PunchFail = true
		log.Printf("Room %s: punch FAILED from %s", code, from)

		// If both failed, offer relay
		if room.Host.PunchFail && room.Client != nil && room.Client.PunchFail {
			s.offerRelay(room, code)
		}
	}
}

func (s *Server) offerRelay(room *Room, code string) {
	port := s.allocateRelayPort()
	if port == 0 {
		log.Printf("Room %s: no relay ports available", code)
		return
	}
	room.RelayPort = port

	log.Printf("Room %s: offering relay on port %d", code, port)

	// Start relay goroutine
	go s.runRelay(room, code, port)

	// Tell both peers to use the relay
	msg := make([]byte, 1+RoomCodeLen+2)
	msg[0] = MsgUseRelay
	copy(msg[1:], code)
	binary.BigEndian.PutUint16(msg[1+RoomCodeLen:], uint16(port))

	s.conn.WriteToUDP(msg, room.Host.Addr)
	if room.Client != nil {
		s.conn.WriteToUDP(msg, room.Client.Addr)
	}
}

func (s *Server) runRelay(room *Room, code string, port int) {
	addr := &net.UDPAddr{Port: port}
	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Printf("Room %s: failed to start relay on port %d: %v", code, port, err)
		s.mu.Lock()
		delete(s.relayPorts, port)
		s.mu.Unlock()
		return
	}
	defer func() {
		conn.Close()
		s.mu.Lock()
		delete(s.relayPorts, port)
		s.mu.Unlock()
		log.Printf("Room %s: relay on port %d stopped", code, port)
	}()

	// Set deadline so relay doesn't live forever
	conn.SetDeadline(time.Now().Add(10 * time.Minute))

	buf := make([]byte, 2048)
	var peerA, peerB *net.UDPAddr

	for {
		n, from, err := conn.ReadFromUDP(buf)
		if err != nil {
			return
		}

		// First two senders become the two relay peers
		if peerA == nil {
			peerA = from
		} else if peerB == nil && !udpAddrEqual(from, peerA) {
			peerB = from
		}

		// Forward to the other peer
		var dest *net.UDPAddr
		if udpAddrEqual(from, peerA) {
			dest = peerB
		} else if udpAddrEqual(from, peerB) {
			dest = peerA
		}

		if dest != nil {
			conn.WriteToUDP(buf[:n], dest)
		}
	}
}

func udpAddrEqual(a, b *net.UDPAddr) bool {
	if a == nil || b == nil {
		return false
	}
	// Normalize to 4-byte IPv4 if it's an IPv4-mapped IPv6 address
	aIP := a.IP.To4()
	if aIP == nil {
		aIP = a.IP
	}
	bIP := b.IP.To4()
	if bIP == nil {
		bIP = b.IP
	}
	return aIP.Equal(bIP) && a.Port == b.Port
}

func (s *Server) allocateRelayPort() int {
	for p := s.relayPortBase; p < s.relayPortMax; p++ {
		if !s.relayPorts[p] {
			s.relayPorts[p] = true
			return p
		}
	}
	return 0
}

func (s *Server) sendPeerAddr(to *net.UDPAddr, code string, addr PeerAddr) {
	ipLen := len(addr.IP)
	msg := make([]byte, 1+RoomCodeLen+1+2+ipLen)
	msg[0] = MsgPeerAddr
	copy(msg[1:], code)
	msg[1+RoomCodeLen] = addr.Type
	binary.BigEndian.PutUint16(msg[1+RoomCodeLen+1:], addr.Port)
	copy(msg[1+RoomCodeLen+3:], addr.IP)
	s.conn.WriteToUDP(msg, to)
}

func (s *Server) sendStartPunch(room *Room, code string) {
	msg := make([]byte, 1+RoomCodeLen)
	msg[0] = MsgStartPunch
	copy(msg[1:], code)
	s.conn.WriteToUDP(msg, room.Host.Addr)
	if room.Client != nil {
		s.conn.WriteToUDP(msg, room.Client.Addr)
	}
}

func (s *Server) sendError(to *net.UDPAddr, code byte, message string) {
	msg := make([]byte, 2+len(message))
	msg[0] = MsgError
	msg[1] = code
	copy(msg[2:], message)
	s.conn.WriteToUDP(msg, to)
}

func (s *Server) findPeer(room *Room, addr *net.UDPAddr) *Peer {
	if room.Host != nil && udpAddrEqual(room.Host.Addr, addr) {
		return room.Host
	}
	if room.Client != nil && udpAddrEqual(room.Client.Addr, addr) {
		return room.Client
	}
	return nil
}

func (s *Server) otherPeer(room *Room, peer *Peer) *Peer {
	if peer == room.Host {
		return room.Client
	}
	return room.Host
}

func (s *Server) generateRoomCode() string {
	const chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789" // no I/O/0/1 to avoid confusion
	b := make([]byte, RoomCodeLen)
	rand.Read(b)
	for i := range b {
		b[i] = chars[b[i]%byte(len(chars))]
	}
	code := string(b)
	// Ensure unique
	if _, exists := s.rooms[code]; exists {
		return s.generateRoomCode()
	}
	return code
}

func (s *Server) cleanupLoop() {
	ticker := time.NewTicker(cleanupPeriod)
	defer ticker.Stop()
	for range ticker.C {
		s.mu.Lock()
		now := time.Now()
		for code, room := range s.rooms {
			if now.Sub(room.LastSeen) > roomTTL {
				log.Printf("Room %s expired", code)
				// Notify peers
				msg := make([]byte, 1+RoomCodeLen)
				msg[0] = MsgRoomExpired
				copy(msg[1:], code)
				if room.Host != nil {
					s.conn.WriteToUDP(msg, room.Host.Addr)
				}
				if room.Client != nil {
					s.conn.WriteToUDP(msg, room.Client.Addr)
				}
				delete(s.rooms, code)
			}
		}
		s.mu.Unlock()
	}
}
