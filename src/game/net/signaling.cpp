#include "signaling.hpp"
#include "netutil.hpp"

#include <enet.h>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

using netutil::nowMs;

namespace proto {
  constexpr uint8_t CreateRoom  = 0x01;
  constexpr uint8_t JoinRoom    = 0x02;
  constexpr uint8_t ReportAddr  = 0x03;
  constexpr uint8_t PunchOK     = 0x04;
  constexpr uint8_t PunchFail   = 0x05;
  constexpr uint8_t Keepalive   = 0x06;
  constexpr uint8_t IceCredentials = 0x07;
  constexpr uint8_t IceCandidate   = 0x08;
  constexpr uint8_t IceGatherDone  = 0x09;

  constexpr uint8_t RoomCreated = 0x81;
  constexpr uint8_t PeerJoined  = 0x82;
  constexpr uint8_t PeerAddr    = 0x83;
  constexpr uint8_t StartPunch  = 0x84;
  constexpr uint8_t UseRelay    = 0x85;
  constexpr uint8_t RoomExpired = 0x86;
  constexpr uint8_t PeerCredentials = 0x87;
  constexpr uint8_t PeerCandidate   = 0x88;
  constexpr uint8_t PeerGatherDone  = 0x89;
  constexpr uint8_t Error       = 0x8F;

  constexpr int RoomCodeLen = 6;
  constexpr uint8_t AddrIPv4 = 4;
  constexpr uint8_t AddrIPv6 = 6;
}

SignalingClient::SignalingClient()
    : sock_(ENET_SOCKET_NULL), state_(Idle), serverPort_(0), relayPort_(0) {}

SignalingClient::~SignalingClient() {
  disconnect();
}

bool SignalingClient::connect(const std::string& serverAddr, uint16_t serverPort) {
  enet_initialize();

  ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (sock == ENET_SOCKET_NULL) {
    fprintf(stderr, "[signaling] ERROR: failed to create socket\n");
    return false;
  }

  enet_socket_set_option(sock, ENET_SOCKOPT_IPV6_V6ONLY, 0);

  ENetAddress anyAddr = {};
  anyAddr.port = 0;
  memset(&anyAddr.host, 0, sizeof(anyAddr.host));
  enet_socket_bind(sock, &anyAddr);

  ENetAddress resolved = {};
  resolved.port = serverPort;
  if (enet_address_set_host(&resolved, serverAddr.c_str()) != 0) {
    fprintf(stderr, "[signaling] ERROR: failed to resolve '%s'\n", serverAddr.c_str());
    enet_socket_destroy(sock);
    return false;
  }

  sock_ = sock;
  serverAddr_ = serverAddr;
  serverPort_ = serverPort;
  resolvedAddr_ = resolved;
  return true;
}

void SignalingClient::disconnect() {
  if (sock_ != ENET_SOCKET_NULL) {
    enet_socket_destroy(sock_);
    sock_ = ENET_SOCKET_NULL;
  }
  state_ = Idle;
  roomCode_.clear();
  peerCandidates_.clear();
  relayPort_ = 0;
}

void SignalingClient::send(const void* data, size_t len) {
  if (sock_ == ENET_SOCKET_NULL) return;
  ENetBuffer buf;
  buf.data = const_cast<void*>(data);
  buf.dataLength = len;
  enet_socket_send(sock_, &resolvedAddr_, &buf, 1);
}

bool SignalingClient::createRoom(const std::string& serverAddr, uint16_t serverPort) {
  if (!connect(serverAddr, serverPort)) return false;
  uint8_t msg[1 + proto::RoomCodeLen] = {};
  msg[0] = proto::CreateRoom;
  send(msg, sizeof(msg));
  state_ = Creating;
  lastSendMs_ = nowMs();
  retryCount_ = 0;
  return true;
}

bool SignalingClient::joinRoom(const std::string& serverAddr, uint16_t serverPort,
                               const std::string& roomCode) {
  if (!connect(serverAddr, serverPort)) return false;
  if (roomCode.size() != proto::RoomCodeLen) return false;

  uint8_t msg[1 + proto::RoomCodeLen];
  msg[0] = proto::JoinRoom;
  std::memcpy(msg + 1, roomCode.data(), proto::RoomCodeLen);
  send(msg, sizeof(msg));
  roomCode_ = roomCode;
  state_ = Joining;
  lastSendMs_ = nowMs();
  retryCount_ = 0;
  return true;
}

void SignalingClient::reportAddress(uint8_t addrType, const std::string& ip, uint16_t port) {
  int ipLen = (addrType == proto::AddrIPv4) ? 4 : 16;
  std::vector<uint8_t> msg(1 + proto::RoomCodeLen + 1 + 2 + ipLen);
  msg[0] = proto::ReportAddr;
  std::memcpy(msg.data() + 1, roomCode_.data(), proto::RoomCodeLen);
  msg[1 + proto::RoomCodeLen] = addrType;
  msg[1 + proto::RoomCodeLen + 1] = (uint8_t)(port >> 8);
  msg[1 + proto::RoomCodeLen + 2] = (uint8_t)(port & 0xFF);

  uint8_t ipBytes[16] = {};
  if (addrType == proto::AddrIPv4)
    inet_pton(AF_INET, ip.c_str(), ipBytes);
  else
    inet_pton(AF_INET6, ip.c_str(), ipBytes);
  std::memcpy(msg.data() + 1 + proto::RoomCodeLen + 3, ipBytes, ipLen);
  send(msg.data(), msg.size());
}

void SignalingClient::reportPunchOK() {
  uint8_t msg[1 + proto::RoomCodeLen];
  msg[0] = proto::PunchOK;
  std::memcpy(msg + 1, roomCode_.data(), proto::RoomCodeLen);
  send(msg, sizeof(msg));
  state_ = Done;
}

void SignalingClient::reportPunchFail() {
  uint8_t msg[1 + proto::RoomCodeLen];
  msg[0] = proto::PunchFail;
  std::memcpy(msg + 1, roomCode_.data(), proto::RoomCodeLen);
  send(msg, sizeof(msg));
}

void SignalingClient::sendIceCredentials(const std::string& ufrag, const std::string& pwd) {
  // Format: [0x07] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
  std::vector<uint8_t> msg(1 + proto::RoomCodeLen + 1 + ufrag.size() + 1 + pwd.size());
  size_t off = 0;
  msg[off++] = proto::IceCredentials;
  std::memcpy(msg.data() + off, roomCode_.data(), proto::RoomCodeLen);
  off += proto::RoomCodeLen;
  msg[off++] = (uint8_t)ufrag.size();
  std::memcpy(msg.data() + off, ufrag.data(), ufrag.size());
  off += ufrag.size();
  msg[off++] = (uint8_t)pwd.size();
  std::memcpy(msg.data() + off, pwd.data(), pwd.size());
  send(msg.data(), msg.size());
}

void SignalingClient::sendIceCandidate(const std::string& sdpCandidate) {
  // Format: [0x08] + [6: room code] + [2: candidate_len BE] + [N: candidate]
  std::vector<uint8_t> msg(1 + proto::RoomCodeLen + 2 + sdpCandidate.size());
  size_t off = 0;
  msg[off++] = proto::IceCandidate;
  std::memcpy(msg.data() + off, roomCode_.data(), proto::RoomCodeLen);
  off += proto::RoomCodeLen;
  uint16_t candLen = (uint16_t)sdpCandidate.size();
  msg[off++] = (uint8_t)(candLen >> 8);
  msg[off++] = (uint8_t)(candLen & 0xFF);
  std::memcpy(msg.data() + off, sdpCandidate.data(), sdpCandidate.size());
  send(msg.data(), msg.size());
}

void SignalingClient::sendIceGatherDone() {
  // Format: [0x09] + [6: room code]
  uint8_t msg[1 + proto::RoomCodeLen];
  msg[0] = proto::IceGatherDone;
  std::memcpy(msg + 1, roomCode_.data(), proto::RoomCodeLen);
  send(msg, sizeof(msg));
}

void SignalingClient::sendKeepalive() {
  uint8_t msg[1 + proto::RoomCodeLen];
  msg[0] = proto::Keepalive;
  std::memcpy(msg + 1, roomCode_.data(), proto::RoomCodeLen);
  send(msg, sizeof(msg));
}

void SignalingClient::poll() {
  if (sock_ == ENET_SOCKET_NULL) return;

  // Retry unacknowledged messages
  if ((state_ == Creating || state_ == Joining) && retryCount_ < kMaxRetries) {
    uint64_t elapsed = nowMs() - lastSendMs_;
    if (elapsed >= (uint64_t)kRetryIntervalMs) {
      retryCount_++;
      uint8_t msg[1 + proto::RoomCodeLen] = {};
      msg[0] = (state_ == Creating) ? proto::CreateRoom : proto::JoinRoom;
      if (state_ == Joining)
        std::memcpy(msg + 1, roomCode_.data(), proto::RoomCodeLen);
      send(msg, sizeof(msg));
      lastSendMs_ = nowMs();
    }
  }

  // Drain all available packets from the socket
  uint8_t recvData[2048];
  ENetBuffer recvBuf;
  recvBuf.data = recvData;
  recvBuf.dataLength = sizeof(recvData);

  for (;;) {
    enet_uint32 waitCondition = ENET_SOCKET_WAIT_RECEIVE;
    if (enet_socket_wait(sock_, &waitCondition, 0) != 0) break;
    if (!(waitCondition & ENET_SOCKET_WAIT_RECEIVE)) break;

    ENetAddress fromAddr = {};
    int recvLen = enet_socket_receive(sock_, &fromAddr, &recvBuf, 1);
    if (recvLen <= 0) break;

    handleMessage(recvData, (size_t)recvLen);
  }
}

void SignalingClient::handleMessage(const uint8_t* data, size_t len) {
  if (len < 1) return;
  uint8_t type = data[0];

  switch (type) {
    case proto::RoomCreated: {
      if (len < 1 + proto::RoomCodeLen) break;
      roomCode_ = std::string((const char*)data + 1, proto::RoomCodeLen);
      // Parse optional TURN credentials: [1: turn_user_len] + [N: turn_user] + [1: turn_pass_len] + [N: turn_pass]
      size_t off = 1 + proto::RoomCodeLen;
      if (off + 1 <= len) {
        uint8_t userLen = data[off++];
        if (off + userLen + 1 <= len) {
          turnUser_ = std::string((const char*)data + off, userLen);
          off += userLen;
          uint8_t passLen = data[off++];
          if (off + passLen <= len) {
            turnPassword_ = std::string((const char*)data + off, passLen);
          }
        }
      }
      state_ = Hosting;
      if (onRoomCreated) onRoomCreated(roomCode_);
      break;
    }
    case proto::PeerJoined: {
      // Parse optional TURN credentials
      size_t off = 1 + proto::RoomCodeLen;
      if (off + 1 <= len) {
        uint8_t userLen = data[off++];
        if (off + userLen + 1 <= len) {
          turnUser_ = std::string((const char*)data + off, userLen);
          off += userLen;
          uint8_t passLen = data[off++];
          if (off + passLen <= len) {
            turnPassword_ = std::string((const char*)data + off, passLen);
          }
        }
      }
      if (state_ == Joining) {
        state_ = WaitingForPeer;
        if (onJoinAcked) onJoinAcked();
      } else {
        if (onPeerJoined) onPeerJoined();
      }
      break;
    }
    case proto::PeerAddr: {
      if (len < 1 + proto::RoomCodeLen + 1 + 2 + 4) break;
      uint8_t addrType = data[1 + proto::RoomCodeLen];
      uint16_t port = (uint16_t)(data[1 + proto::RoomCodeLen + 1] << 8 |
                                  data[1 + proto::RoomCodeLen + 2]);
      int ipLen = (addrType == proto::AddrIPv4) ? 4 : 16;
      if ((int)len < 1 + proto::RoomCodeLen + 3 + ipLen) break;

      char ipStr[INET6_ADDRSTRLEN] = {};
      if (addrType == proto::AddrIPv4)
        inet_ntop(AF_INET, data + 1 + proto::RoomCodeLen + 3, ipStr, sizeof(ipStr));
      else
        inet_ntop(AF_INET6, data + 1 + proto::RoomCodeLen + 3, ipStr, sizeof(ipStr));

      PeerCandidate cand{addrType, ipStr, port};
      peerCandidates_.push_back(cand);
      if (onPeerAddr) onPeerAddr(cand);
      break;
    }
    case proto::StartPunch: {
      state_ = Punching;
      if (onStartPunch) onStartPunch();
      break;
    }
    case proto::UseRelay: {
      if (len < 1 + proto::RoomCodeLen + 2 + 8) break;
      relayPort_ = (uint16_t)(data[1 + proto::RoomCodeLen] << 8 |
                               data[1 + proto::RoomCodeLen + 1]);
      relayToken_.assign(data + 1 + proto::RoomCodeLen + 2,
                         data + 1 + proto::RoomCodeLen + 2 + 8);
      state_ = Relaying;
      if (onUseRelay) onUseRelay(relayPort_);
      break;
    }
    case proto::RoomExpired: {
      fprintf(stderr, "[signaling] room expired\n");
      state_ = Failed;
      if (onRoomExpired) onRoomExpired();
      break;
    }
    case proto::Error: {
      std::string msg;
      if (len > 2)
        msg = std::string((const char*)data + 2, len - 2);
      fprintf(stderr, "[signaling] ERROR from server: %s\n", msg.c_str());
      state_ = Failed;
      if (onError) onError(msg);
      break;
    }
    case proto::PeerCredentials: {
      // Format: [0x87] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
      size_t off = 1 + proto::RoomCodeLen;
      if (off + 1 > len) break;
      uint8_t ufragLen = data[off++];
      if (off + ufragLen + 1 > len) break;
      std::string ufrag((const char*)data + off, ufragLen);
      off += ufragLen;
      uint8_t pwdLen = data[off++];
      if (off + pwdLen > len) break;
      std::string pwd((const char*)data + off, pwdLen);
      state_ = IceExchanging;
      if (onPeerCredentials) onPeerCredentials(ufrag, pwd);
      break;
    }
    case proto::PeerCandidate: {
      // Format: [0x88] + [6: room code] + [2: candidate_len BE] + [N: candidate]
      size_t off = 1 + proto::RoomCodeLen;
      if (off + 2 > len) break;
      uint16_t candLen = (uint16_t)(data[off] << 8 | data[off + 1]);
      off += 2;
      if (off + candLen > len) break;
      std::string candidate((const char*)data + off, candLen);
      if (onPeerCandidate) onPeerCandidate(candidate);
      break;
    }
    case proto::PeerGatherDone: {
      if (onPeerGatherDone) onPeerGatherDone();
      break;
    }
    default:
      break;
  }
}
