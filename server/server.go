package main

import (
	"crypto/hmac"
	"crypto/rand"
	"crypto/sha1"
	"encoding/base64"
	"encoding/binary"
	"fmt"
	"log"
	"net"
	"os"
	"sync"
	"time"
)

const (
	roomTTL       = 5 * time.Minute
	cleanupPeriod = 10 * time.Second
	maxRooms      = 1000
)

// Peer represents one connected game client.
type Peer struct {
	Addr *net.UDPAddr // UDP source address for sending replies
}

// Room holds two peers trying to connect via ICE.
type Room struct {
	Code      [RoomCodeLen]byte
	Host      *Peer
	Client    *Peer
	CreatedAt time.Time
	LastSeen  time.Time
}

type Server struct {
	conn       *net.UDPConn
	mu         sync.Mutex
	rooms      map[string]*Room // code string → room
	turnSecret string           // shared secret for TURN credential generation (empty = no TURN)
}

func NewServer(conn *net.UDPConn) *Server {
	return &Server{
		conn:       conn,
		rooms:      make(map[string]*Room),
		turnSecret: os.Getenv("TURN_SECRET"),
	}
}

func (s *Server) Run() {
	go s.cleanupLoop()

	buf := make([]byte, 2048)
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
	case MsgIceCredentials:
		s.handleIceCredentials(data, from)
	case MsgIceCandidate:
		s.handleIceCandidate(data, from)
	case MsgIceGatherDone:
		s.handleIceGatherDone(data, from)
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
	case MsgReportAddr, MsgPunchOK, MsgPunchFail:
		// Legacy messages — ignore silently
	default:
		s.sendError(from, ErrInvalidMsg, "unknown type")
	}
}

func (s *Server) handleCreateRoom(from *net.UDPAddr) {
	s.mu.Lock()

	if len(s.rooms) >= maxRooms {
		s.mu.Unlock()
		s.sendError(from, ErrRoomFull, "server full")
		return
	}

	code := s.generateRoomCode()
	if code == "" {
		s.mu.Unlock()
		s.sendError(from, ErrRoomFull, "server full")
		return
	}
	now := time.Now()
	room := &Room{
		Host:      &Peer{Addr: from},
		CreatedAt: now,
		LastSeen:  now,
	}
	copy(room.Code[:], code)
	s.rooms[code] = room

	s.mu.Unlock()

	log.Printf("Room %s created by %s", code, from)

	// Send RoomCreated with TURN credentials
	s.sendRoomResponse(MsgRoomCreated, code, from)
}

func (s *Server) handleJoinRoom(code string, from *net.UDPAddr) {
	s.mu.Lock()

	room, ok := s.rooms[code]
	if !ok {
		s.mu.Unlock()
		s.sendError(from, ErrRoomNotFound, "no such room")
		return
	}
	if room.Client != nil {
		s.mu.Unlock()
		s.sendError(from, ErrRoomFull, "room full")
		return
	}

	room.Client = &Peer{Addr: from}
	room.LastSeen = time.Now()
	hostAddr := room.Host.Addr

	s.mu.Unlock()

	log.Printf("Room %s: client joined from %s", code, from)

	// Acknowledge to joining client (with TURN credentials)
	s.sendRoomResponse(MsgPeerJoined, code, from)

	// Notify host (with TURN credentials)
	s.sendRoomResponse(MsgPeerJoined, code, hostAddr)
}

func (s *Server) handleIceCredentials(data []byte, from *net.UDPAddr) {
	// Format: [0x07] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
	if len(data) < 1+RoomCodeLen+1 {
		return
	}
	code := string(data[1 : 1+RoomCodeLen])

	s.mu.Lock()
	room, ok := s.rooms[code]
	if !ok {
		s.mu.Unlock()
		s.sendError(from, ErrRoomNotFound, "no such room")
		return
	}
	room.LastSeen = time.Now()
	other := s.otherPeerAddr(room, from)
	s.mu.Unlock()

	if other == nil {
		return
	}

	// Parse credentials from the message
	off := 1 + RoomCodeLen
	if off >= len(data) {
		return
	}
	ufragLen := int(data[off])
	off++
	if off+ufragLen+1 > len(data) {
		return
	}
	ufrag := data[off : off+ufragLen]
	off += ufragLen
	pwdLen := int(data[off])
	off++
	if off+pwdLen > len(data) {
		return
	}
	pwd := data[off : off+pwdLen]

	// Forward as PeerCredentials to the other peer
	msg := make([]byte, 1+RoomCodeLen+1+len(ufrag)+1+len(pwd))
	i := 0
	msg[i] = MsgPeerCredentials
	i++
	copy(msg[i:], code)
	i += RoomCodeLen
	msg[i] = byte(len(ufrag))
	i++
	copy(msg[i:], ufrag)
	i += len(ufrag)
	msg[i] = byte(len(pwd))
	i++
	copy(msg[i:], pwd)
	s.conn.WriteToUDP(msg, other)
}

func (s *Server) handleIceCandidate(data []byte, from *net.UDPAddr) {
	// Format: [0x08] + [6: room code] + [2: candidate_len BE] + [N: candidate]
	if len(data) < 1+RoomCodeLen+2 {
		return
	}
	code := string(data[1 : 1+RoomCodeLen])

	s.mu.Lock()
	room, ok := s.rooms[code]
	if !ok {
		s.mu.Unlock()
		return
	}
	room.LastSeen = time.Now()
	other := s.otherPeerAddr(room, from)
	s.mu.Unlock()

	if other == nil {
		return
	}

	off := 1 + RoomCodeLen
	candLen := int(binary.BigEndian.Uint16(data[off:]))
	off += 2
	if off+candLen > len(data) {
		return
	}

	// Forward as PeerCandidate
	msg := make([]byte, 1+RoomCodeLen+2+candLen)
	msg[0] = MsgPeerCandidate
	copy(msg[1:], code)
	binary.BigEndian.PutUint16(msg[1+RoomCodeLen:], uint16(candLen))
	copy(msg[1+RoomCodeLen+2:], data[off:off+candLen])
	s.conn.WriteToUDP(msg, other)
}

func (s *Server) handleIceGatherDone(data []byte, from *net.UDPAddr) {
	// Format: [0x09] + [6: room code]
	if len(data) < 1+RoomCodeLen {
		return
	}
	code := string(data[1 : 1+RoomCodeLen])

	s.mu.Lock()
	room, ok := s.rooms[code]
	if !ok {
		s.mu.Unlock()
		return
	}
	room.LastSeen = time.Now()
	other := s.otherPeerAddr(room, from)
	s.mu.Unlock()

	if other == nil {
		return
	}

	// Forward as PeerGatherDone
	msg := make([]byte, 1+RoomCodeLen)
	msg[0] = MsgPeerGatherDone
	copy(msg[1:], code)
	s.conn.WriteToUDP(msg, other)
}

func (s *Server) sendRoomResponse(msgType byte, code string, to *net.UDPAddr) {
	turnUser, turnPass := s.generateTurnCredentials()
	msg := make([]byte, 1+RoomCodeLen+1+len(turnUser)+1+len(turnPass))
	i := 0
	msg[i] = msgType
	i++
	copy(msg[i:], code)
	i += RoomCodeLen
	msg[i] = byte(len(turnUser))
	i++
	copy(msg[i:], turnUser)
	i += len(turnUser)
	msg[i] = byte(len(turnPass))
	i++
	copy(msg[i:], turnPass)
	s.conn.WriteToUDP(msg, to)
}

func (s *Server) generateTurnCredentials() (user, pass string) {
	if s.turnSecret == "" {
		return "", ""
	}
	expiry := time.Now().Add(24 * time.Hour).Unix()
	user = fmt.Sprintf("%d", expiry)
	mac := hmac.New(sha1.New, []byte(s.turnSecret))
	mac.Write([]byte(user))
	pass = base64.StdEncoding.EncodeToString(mac.Sum(nil))
	return
}

func (s *Server) otherPeerAddr(room *Room, from *net.UDPAddr) *net.UDPAddr {
	if room.Host != nil && udpAddrEqual(room.Host.Addr, from) {
		if room.Client != nil {
			return room.Client.Addr
		}
	}
	if room.Client != nil && udpAddrEqual(room.Client.Addr, from) {
		return room.Host.Addr
	}
	return nil
}

func (s *Server) sendError(to *net.UDPAddr, code byte, message string) {
	msg := make([]byte, 2+len(message))
	msg[0] = MsgError
	msg[1] = code
	copy(msg[2:], message)
	s.conn.WriteToUDP(msg, to)
}

func (s *Server) generateRoomCode() string {
	const chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
	const maxAttempts = 100
	b := make([]byte, RoomCodeLen)
	for attempt := 0; attempt < maxAttempts; attempt++ {
		rand.Read(b)
		for i := range b {
			b[i] = chars[b[i]%byte(len(chars))]
		}
		code := string(b)
		if _, exists := s.rooms[code]; !exists {
			return code
		}
	}
	return ""
}

func (s *Server) cleanupLoop() {
	ticker := time.NewTicker(cleanupPeriod)
	defer ticker.Stop()
	for range ticker.C {
		type expiredRoom struct {
			code       string
			hostAddr   *net.UDPAddr
			clientAddr *net.UDPAddr
		}
		var expired []expiredRoom

		s.mu.Lock()
		now := time.Now()
		for code, room := range s.rooms {
			if now.Sub(room.LastSeen) > roomTTL {
				log.Printf("Room %s expired", code)
				er := expiredRoom{code: code}
				if room.Host != nil {
					er.hostAddr = room.Host.Addr
				}
				if room.Client != nil {
					er.clientAddr = room.Client.Addr
				}
				expired = append(expired, er)
				delete(s.rooms, code)
			}
		}
		s.mu.Unlock()

		for _, er := range expired {
			msg := make([]byte, 1+RoomCodeLen)
			msg[0] = MsgRoomExpired
			copy(msg[1:], er.code)
			if er.hostAddr != nil {
				s.conn.WriteToUDP(msg, er.hostAddr)
			}
			if er.clientAddr != nil {
				s.conn.WriteToUDP(msg, er.clientAddr)
			}
		}
	}
}

func udpAddrEqual(a, b *net.UDPAddr) bool {
	return a.Port == b.Port && a.IP.Equal(b.IP)
}