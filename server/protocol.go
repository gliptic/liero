package main

// Binary protocol for signaling between game clients and this server.
// All messages are UDP datagrams with a 1-byte type prefix.
//
// Client → Server:
//   CreateRoom  [0x01] + [6 bytes: reserved]
//   JoinRoom    [0x02] + [6 bytes: room code ASCII]
//   ReportAddr  [0x03] + [6 bytes: room code] + [1 byte: addr_type (4=ipv4, 6=ipv6)] + [2 bytes: port BE] + [N bytes: IP (4 or 16)]
//   PunchOK     [0x04] + [6 bytes: room code]
//   PunchFail   [0x05] + [6 bytes: room code]
//   Keepalive   [0x06] + [6 bytes: room code]
//
// Server → Client:
//   RoomCreated [0x81] + [6 bytes: room code]
//   PeerJoined  [0x82] + [6 bytes: room code]
//   PeerAddr    [0x83] + [6 bytes: room code] + [1 byte: addr_type] + [2 bytes: port BE] + [N bytes: IP]
//   StartPunch  [0x84] + [6 bytes: room code]
//   UseRelay    [0x85] + [6 bytes: room code] + [2 bytes: relay port BE]
//   Error       [0x8F] + [1 byte: error code] + [N bytes: message]
//   RoomExpired [0x86] + [6 bytes: room code]
//
// Error codes:
//   0x01 = room not found
//   0x02 = room full
//   0x03 = invalid message

const (
	// Client → Server
	MsgCreateRoom byte = 0x01
	MsgJoinRoom   byte = 0x02
	MsgReportAddr byte = 0x03
	MsgPunchOK    byte = 0x04
	MsgPunchFail  byte = 0x05
	MsgKeepalive  byte = 0x06

	// Server → Client
	MsgRoomCreated byte = 0x81
	MsgPeerJoined  byte = 0x82
	MsgPeerAddr    byte = 0x83
	MsgStartPunch  byte = 0x84
	MsgUseRelay    byte = 0x85
	MsgRoomExpired byte = 0x86
	MsgError       byte = 0x8F

	// Error codes
	ErrRoomNotFound byte = 0x01
	ErrRoomFull     byte = 0x02
	ErrInvalidMsg   byte = 0x03

	// Address types
	AddrIPv4 byte = 4
	AddrIPv6 byte = 6

	RoomCodeLen = 6
)
