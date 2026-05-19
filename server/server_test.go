package main

import (
	"encoding/binary"
	"net"
	"testing"
	"time"
)

func startTestServer(t *testing.T) (*Server, *net.UDPAddr) {
	t.Helper()
	addr := &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: 0}
	conn, err := net.ListenUDP("udp4", addr)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { conn.Close() })

	srv := NewServer(conn, 0, 0) // no relay ports in basic tests
	go srv.Run()

	return srv, conn.LocalAddr().(*net.UDPAddr)
}

func dial(t *testing.T, srvAddr *net.UDPAddr) *net.UDPConn {
	t.Helper()
	conn, err := net.DialUDP("udp4", nil, srvAddr)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { conn.Close() })
	return conn
}

func readMsg(t *testing.T, conn *net.UDPConn) []byte {
	t.Helper()
	conn.SetReadDeadline(time.Now().Add(2 * time.Second))
	buf := make([]byte, 512)
	n, err := conn.Read(buf)
	if err != nil {
		t.Fatal("read timeout:", err)
	}
	return buf[:n]
}

func TestCreateAndJoinRoom(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)
	client := dial(t, srvAddr)

	// Host creates room
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	if resp[0] != MsgRoomCreated {
		t.Fatalf("expected RoomCreated (0x81), got 0x%02x", resp[0])
	}
	if len(resp) < 1+RoomCodeLen {
		t.Fatal("response too short")
	}
	code := resp[1 : 1+RoomCodeLen]
	t.Logf("Room code: %s", string(code))

	// Client joins room
	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)

	// Host should receive PeerJoined
	hostMsg := readMsg(t, host)
	if hostMsg[0] != MsgPeerJoined {
		t.Fatalf("expected PeerJoined (0x82), got 0x%02x", hostMsg[0])
	}
}

func TestAddressExchange(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)
	client := dial(t, srvAddr)

	// Create room
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	code := resp[1 : 1+RoomCodeLen]

	// Host reports its address
	addrMsg := make([]byte, 1+RoomCodeLen+1+2+4)
	addrMsg[0] = MsgReportAddr
	copy(addrMsg[1:], code)
	addrMsg[1+RoomCodeLen] = AddrIPv4
	binary.BigEndian.PutUint16(addrMsg[1+RoomCodeLen+1:], 19532)
	copy(addrMsg[1+RoomCodeLen+3:], net.IPv4(1, 2, 3, 4).To4())
	host.Write(addrMsg)

	// Client joins
	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)

	// Client should receive host's PeerAddr
	clientMsg := readMsg(t, client)
	if clientMsg[0] != MsgPeerAddr {
		t.Fatalf("expected PeerAddr (0x83), got 0x%02x", clientMsg[0])
	}
	addrType := clientMsg[1+RoomCodeLen]
	if addrType != AddrIPv4 {
		t.Fatalf("expected IPv4 addr type, got %d", addrType)
	}
	port := binary.BigEndian.Uint16(clientMsg[1+RoomCodeLen+1:])
	if port != 19532 {
		t.Fatalf("expected port 19532, got %d", port)
	}

	// Client should also receive StartPunch (host already has addresses)
	clientMsg2 := readMsg(t, client)
	if clientMsg2[0] != MsgStartPunch {
		t.Fatalf("expected StartPunch (0x84), got 0x%02x", clientMsg2[0])
	}

	// Host should have received PeerJoined + StartPunch
	hostMsg := readMsg(t, host)
	if hostMsg[0] != MsgPeerJoined {
		t.Fatalf("expected PeerJoined, got 0x%02x", hostMsg[0])
	}
	hostMsg2 := readMsg(t, host)
	if hostMsg2[0] != MsgStartPunch {
		t.Fatalf("expected StartPunch, got 0x%02x", hostMsg2[0])
	}
}

func TestJoinNonexistentRoom(t *testing.T) {
	_, srvAddr := startTestServer(t)
	client := dial(t, srvAddr)

	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], "XXXXXX")
	client.Write(joinMsg)

	resp := readMsg(t, client)
	if resp[0] != MsgError {
		t.Fatalf("expected Error (0x8F), got 0x%02x", resp[0])
	}
	if resp[1] != ErrRoomNotFound {
		t.Fatalf("expected ErrRoomNotFound, got 0x%02x", resp[1])
	}
}
