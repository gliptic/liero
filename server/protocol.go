package main

// Binary protocol for signaling between game clients and this server.
// All messages are UDP datagrams with a 1-byte type prefix.
//
// Client → Server:
//   CreateRoom      [0x01] + [6 bytes: reserved]
//   JoinRoom        [0x02] + [6 bytes: room code ASCII]
//   Keepalive       [0x06] + [6 bytes: room code]
//   IceCredentials  [0x07] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
//   IceCandidate    [0x08] + [6: room code] + [2: candidate_len BE] + [N: candidate SDP string]
//   IceGatherDone   [0x09] + [6: room code]
//
// Server → Client:
//   RoomCreated     [0x81] + [6: room code] + [1: turn_user_len] + [N: turn_user] + [1: turn_pass_len] + [N: turn_pass]
//   PeerJoined      [0x82] + [6: room code] + [1: turn_user_len] + [N: turn_user] + [1: turn_pass_len] + [N: turn_pass]
//   RoomExpired     [0x86] + [6 bytes: room code]
//   PeerCredentials [0x87] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
//   PeerCandidate   [0x88] + [6: room code] + [2: candidate_len BE] + [N: candidate SDP string]
//   PeerGatherDone  [0x89] + [6: room code]
//   Error           [0x8F] + [1 byte: error code] + [N bytes: message]
//
// Legacy messages (kept for backwards compat, no-ops on server):
//   ReportAddr  [0x03], PunchOK [0x04], PunchFail [0x05]
//
// Error codes:
//   0x01 = room not found
//   0x02 = room full
//   0x03 = invalid message

const (
	// Client → Server
	MsgCreateRoom      byte = 0x01
	MsgJoinRoom        byte = 0x02
	MsgReportAddr      byte = 0x03 // legacy, ignored
	MsgPunchOK         byte = 0x04 // legacy, ignored
	MsgPunchFail       byte = 0x05 // legacy, ignored
	MsgKeepalive       byte = 0x06
	MsgIceCredentials  byte = 0x07
	MsgIceCandidate    byte = 0x08
	MsgIceGatherDone   byte = 0x09

	// Server → Client
	MsgRoomCreated     byte = 0x81
	MsgPeerJoined      byte = 0x82
	MsgRoomExpired     byte = 0x86
	MsgPeerCredentials byte = 0x87
	MsgPeerCandidate   byte = 0x88
	MsgPeerGatherDone  byte = 0x89
	MsgError           byte = 0x8F

	// Error codes
	ErrRoomNotFound byte = 0x01
	ErrRoomFull     byte = 0x02
	ErrInvalidMsg   byte = 0x03

	RoomCodeLen = 6
)
