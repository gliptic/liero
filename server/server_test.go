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

	srv := NewServer(conn)
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
	buf := make([]byte, 2048)
	n, err := conn.Read(buf)
	if err != nil {
		t.Fatal("read timeout:", err)
	}
	return buf[:n]
}

// --- Basic signaling tests ---

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
	if len(resp) < 1+RoomCodeLen+2 { // code + at least 2 bytes for TURN lens
		t.Fatal("response too short")
	}
	code := resp[1 : 1+RoomCodeLen]

	// Client joins room
	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)

	// Client gets PeerJoined
	clientResp := readMsg(t, client)
	if clientResp[0] != MsgPeerJoined {
		t.Fatalf("client expected PeerJoined (0x82), got 0x%02x", clientResp[0])
	}

	// Host gets PeerJoined
	hostMsg := readMsg(t, host)
	if hostMsg[0] != MsgPeerJoined {
		t.Fatalf("host expected PeerJoined (0x82), got 0x%02x", hostMsg[0])
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

func TestIceCredentialExchange(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)
	client := dial(t, srvAddr)

	// Create and join
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	code := resp[1 : 1+RoomCodeLen]

	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)
	readMsg(t, client) // PeerJoined
	readMsg(t, host)   // PeerJoined

	// Host sends ICE credentials
	ufrag := "testufrag"
	pwd := "testpassword123"
	credMsg := make([]byte, 1+RoomCodeLen+1+len(ufrag)+1+len(pwd))
	credMsg[0] = MsgIceCredentials
	copy(credMsg[1:], code)
	off := 1 + RoomCodeLen
	credMsg[off] = byte(len(ufrag))
	off++
	copy(credMsg[off:], ufrag)
	off += len(ufrag)
	credMsg[off] = byte(len(pwd))
	off++
	copy(credMsg[off:], pwd)
	host.Write(credMsg)

	// Client should receive PeerCredentials
	msg := readMsg(t, client)
	if msg[0] != MsgPeerCredentials {
		t.Fatalf("expected PeerCredentials (0x87), got 0x%02x", msg[0])
	}
	// Parse received credentials
	roff := 1 + RoomCodeLen
	ufragLen := int(msg[roff])
	roff++
	receivedUfrag := string(msg[roff : roff+ufragLen])
	roff += ufragLen
	pwdLen := int(msg[roff])
	roff++
	receivedPwd := string(msg[roff : roff+pwdLen])

	if receivedUfrag != ufrag {
		t.Fatalf("ufrag mismatch: got %q want %q", receivedUfrag, ufrag)
	}
	if receivedPwd != pwd {
		t.Fatalf("pwd mismatch: got %q want %q", receivedPwd, pwd)
	}
}

func TestIceCandidateExchange(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)
	client := dial(t, srvAddr)

	// Create and join
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	code := resp[1 : 1+RoomCodeLen]

	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)
	readMsg(t, client)
	readMsg(t, host)

	// Host sends ICE candidate
	candidate := "a=candidate:1 1 UDP 2122252543 192.168.1.100 12345 typ host"
	candMsg := make([]byte, 1+RoomCodeLen+2+len(candidate))
	candMsg[0] = MsgIceCandidate
	copy(candMsg[1:], code)
	binary.BigEndian.PutUint16(candMsg[1+RoomCodeLen:], uint16(len(candidate)))
	copy(candMsg[1+RoomCodeLen+2:], candidate)
	host.Write(candMsg)

	// Client should receive PeerCandidate
	msg := readMsg(t, client)
	if msg[0] != MsgPeerCandidate {
		t.Fatalf("expected PeerCandidate (0x88), got 0x%02x", msg[0])
	}
	candLen := int(binary.BigEndian.Uint16(msg[1+RoomCodeLen:]))
	receivedCand := string(msg[1+RoomCodeLen+2 : 1+RoomCodeLen+2+candLen])
	if receivedCand != candidate {
		t.Fatalf("candidate mismatch: got %q want %q", receivedCand, candidate)
	}
}

func TestIceGatherDoneExchange(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)
	client := dial(t, srvAddr)

	// Create and join
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	code := resp[1 : 1+RoomCodeLen]

	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)
	readMsg(t, client)
	readMsg(t, host)

	// Host sends gather done
	doneMsg := make([]byte, 1+RoomCodeLen)
	doneMsg[0] = MsgIceGatherDone
	copy(doneMsg[1:], code)
	host.Write(doneMsg)

	// Client should receive PeerGatherDone
	msg := readMsg(t, client)
	if msg[0] != MsgPeerGatherDone {
		t.Fatalf("expected PeerGatherDone (0x89), got 0x%02x", msg[0])
	}
}

func TestTurnCredentialsInRoomCreated(t *testing.T) {
	// When TURN_SECRET is set, credentials should be included
	addr := &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: 0}
	conn, err := net.ListenUDP("udp4", addr)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { conn.Close() })

	srv := NewServer(conn)
	srv.turnSecret = "test-secret"
	go srv.Run()

	srvAddr := conn.LocalAddr().(*net.UDPAddr)
	host := dial(t, srvAddr)

	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	if resp[0] != MsgRoomCreated {
		t.Fatalf("expected RoomCreated, got 0x%02x", resp[0])
	}

	// Parse TURN credentials
	off := 1 + RoomCodeLen
	if off >= len(resp) {
		t.Fatal("no TURN credentials in response")
	}
	userLen := int(resp[off])
	off++
	if userLen == 0 {
		t.Fatal("expected non-empty TURN user when secret is set")
	}
	user := string(resp[off : off+userLen])
	off += userLen
	passLen := int(resp[off])
	off++
	if passLen == 0 {
		t.Fatal("expected non-empty TURN password when secret is set")
	}
	_ = string(resp[off : off+passLen])

	// User should be a unix timestamp (expiry)
	if len(user) < 5 {
		t.Fatalf("TURN user too short: %q", user)
	}
}

func TestNoTurnCredentialsWithoutSecret(t *testing.T) {
	_, srvAddr := startTestServer(t) // no TURN_SECRET set

	host := dial(t, srvAddr)
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)

	// Parse: after room code, both lengths should be 0
	off := 1 + RoomCodeLen
	if off >= len(resp) {
		t.Fatal("response too short")
	}
	userLen := int(resp[off])
	off++
	if userLen != 0 {
		t.Fatalf("expected empty TURN user without secret, got len=%d", userLen)
	}
	passLen := int(resp[off])
	if passLen != 0 {
		t.Fatalf("expected empty TURN password without secret, got len=%d", passLen)
	}
}

func TestKeepaliveResetsExpiry(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)

	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	code := resp[1 : 1+RoomCodeLen]

	// Send keepalive
	keepalive := make([]byte, 1+RoomCodeLen)
	keepalive[0] = MsgKeepalive
	copy(keepalive[1:], code)
	host.Write(keepalive)

	// Room should still exist — try to join
	client := dial(t, srvAddr)
	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)

	resp = readMsg(t, client)
	if resp[0] != MsgPeerJoined {
		t.Fatalf("expected PeerJoined after keepalive, got 0x%02x", resp[0])
	}
}

func TestLegacyMessagesIgnored(t *testing.T) {
	_, srvAddr := startTestServer(t)

	host := dial(t, srvAddr)

	// Create room first
	host.Write([]byte{MsgCreateRoom, 0, 0, 0, 0, 0, 0})
	resp := readMsg(t, host)
	code := resp[1 : 1+RoomCodeLen]

	// Send legacy ReportAddr — should be silently ignored
	addrMsg := make([]byte, 1+RoomCodeLen+1+2+4)
	addrMsg[0] = MsgReportAddr
	copy(addrMsg[1:], code)
	host.Write(addrMsg)

	// Send legacy PunchOK — should be silently ignored
	punchMsg := make([]byte, 1+RoomCodeLen)
	punchMsg[0] = MsgPunchOK
	copy(punchMsg[1:], code)
	host.Write(punchMsg)

	// No response expected — verify by trying another valid operation
	// If legacy messages crashed the server, this would fail
	client := dial(t, srvAddr)
	joinMsg := make([]byte, 1+RoomCodeLen)
	joinMsg[0] = MsgJoinRoom
	copy(joinMsg[1:], code)
	client.Write(joinMsg)

	msg := readMsg(t, client)
	if msg[0] != MsgPeerJoined {
		t.Fatalf("expected PeerJoined, got 0x%02x (server may have crashed on legacy msg)", msg[0])
	}
}