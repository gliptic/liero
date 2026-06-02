#include "signaling.hpp"
#include "netutil.hpp"

#include <enet.h>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

using netutil::NowMs;

namespace proto {
constexpr uint8_t kCreateRoom = 0x01;
constexpr uint8_t kJoinRoom = 0x02;
constexpr uint8_t kReportAddr = 0x03;
constexpr uint8_t kPunchOk = 0x04;
constexpr uint8_t kPunchFail = 0x05;
constexpr uint8_t kKeepalive = 0x06;
constexpr uint8_t kIceCredentials = 0x07;
constexpr uint8_t kIceCandidate = 0x08;
constexpr uint8_t kIceGatherDone = 0x09;

constexpr uint8_t kRoomCreated = 0x81;
constexpr uint8_t kPeerJoined = 0x82;
constexpr uint8_t kPeerAddr = 0x83;
constexpr uint8_t kStartPunch = 0x84;
constexpr uint8_t kUseRelay = 0x85;
constexpr uint8_t kRoomExpired = 0x86;
constexpr uint8_t kPeerCredentials = 0x87;
constexpr uint8_t kPeerCandidate = 0x88;
constexpr uint8_t kPeerGatherDone = 0x89;
constexpr uint8_t kError = 0x8F;

constexpr int kRoomCodeLen = 6;
constexpr uint8_t kAddrIPv4 = 4;
constexpr uint8_t kAddrIPv6 = 6;
}  // namespace proto

SignalingClient::SignalingClient()
    : sock_(ENET_SOCKET_NULL), state_(kIdle), serverPort_(0), relayPort_(0) {}

SignalingClient::~SignalingClient() { Disconnect(); }

bool SignalingClient::Connect(const std::string& server_addr, uint16_t server_port) {
  enet_initialize();

  ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
  if (sock == ENET_SOCKET_NULL) {
    fprintf(stderr, "[signaling] ERROR: failed to create socket\n");
    return false;
  }

  enet_socket_set_option(sock, ENET_SOCKOPT_IPV6_V6ONLY, 0);

  ENetAddress any_addr = {};
  any_addr.port = 0;
  memset(&any_addr.host, 0, sizeof(any_addr.host));
  enet_socket_bind(sock, &any_addr);

  ENetAddress resolved = {};
  resolved.port = server_port;
  if (enet_address_set_host(&resolved, server_addr.c_str()) != 0) {
    fprintf(stderr, "[signaling] ERROR: failed to resolve '%s'\n", server_addr.c_str());
    enet_socket_destroy(sock);
    return false;
  }

  sock_ = sock;
  serverAddr_ = server_addr;
  serverPort_ = server_port;
  resolvedAddr_ = resolved;
  return true;
}

void SignalingClient::Disconnect() {
  if (sock_ != ENET_SOCKET_NULL) {
    enet_socket_destroy(sock_);
    sock_ = ENET_SOCKET_NULL;
  }
  state_ = kIdle;
  roomCode_.clear();
  peerCandidates_.clear();
  relayPort_ = 0;
}

void SignalingClient::Send(const void* data, size_t len) {
  if (sock_ == ENET_SOCKET_NULL) return;
  ENetBuffer buf;
  buf.data = const_cast<void*>(data);
  buf.dataLength = len;
  enet_socket_send(sock_, &resolvedAddr_, &buf, 1);
}

bool SignalingClient::CreateRoom(const std::string& server_addr, uint16_t server_port) {
  if (!Connect(server_addr, server_port)) return false;
  uint8_t msg[1 + proto::kRoomCodeLen] = {};
  msg[0] = proto::kCreateRoom;
  Send(msg, sizeof(msg));
  state_ = kCreating;
  lastSendMs_ = NowMs();
  retryCount_ = 0;
  return true;
}

bool SignalingClient::JoinRoom(const std::string& server_addr, uint16_t server_port,
                               const std::string& room_code) {
  if (!Connect(server_addr, server_port)) return false;
  if (room_code.size() != proto::kRoomCodeLen) return false;

  uint8_t msg[1 + proto::kRoomCodeLen];
  msg[0] = proto::kJoinRoom;
  std::memcpy(msg + 1, room_code.data(), proto::kRoomCodeLen);
  Send(msg, sizeof(msg));
  roomCode_ = room_code;
  state_ = kJoining;
  lastSendMs_ = NowMs();
  retryCount_ = 0;
  return true;
}

void SignalingClient::ReportAddress(uint8_t addr_type, const std::string& ip, uint16_t port) {
  int ip_len = (addr_type == proto::kAddrIPv4) ? 4 : 16;
  std::vector<uint8_t> msg(1 + proto::kRoomCodeLen + 1 + 2 + ip_len);
  msg[0] = proto::kReportAddr;
  std::memcpy(msg.data() + 1, roomCode_.data(), proto::kRoomCodeLen);
  msg[1 + proto::kRoomCodeLen] = addr_type;
  msg[1 + proto::kRoomCodeLen + 1] = (uint8_t)(port >> 8);
  msg[1 + proto::kRoomCodeLen + 2] = (uint8_t)(port & 0xFF);

  uint8_t ip_bytes[16] = {};
  if (addr_type == proto::kAddrIPv4)
    inet_pton(AF_INET, ip.c_str(), ip_bytes);
  else
    inet_pton(AF_INET6, ip.c_str(), ip_bytes);
  std::memcpy(msg.data() + 1 + proto::kRoomCodeLen + 3, ip_bytes, ip_len);
  Send(msg.data(), msg.size());
}

void SignalingClient::ReportPunchOk() {
  uint8_t msg[1 + proto::kRoomCodeLen];
  msg[0] = proto::kPunchOk;
  std::memcpy(msg + 1, roomCode_.data(), proto::kRoomCodeLen);
  Send(msg, sizeof(msg));
  state_ = kDone;
}

void SignalingClient::ReportPunchFail() {
  uint8_t msg[1 + proto::kRoomCodeLen];
  msg[0] = proto::kPunchFail;
  std::memcpy(msg + 1, roomCode_.data(), proto::kRoomCodeLen);
  Send(msg, sizeof(msg));
}

void SignalingClient::SendIceCredentials(const std::string& ufrag, const std::string& pwd) {
  // Format: [0x07] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
  std::vector<uint8_t> msg(1 + proto::kRoomCodeLen + 1 + ufrag.size() + 1 + pwd.size());
  size_t off = 0;
  msg[off++] = proto::kIceCredentials;
  std::memcpy(msg.data() + off, roomCode_.data(), proto::kRoomCodeLen);
  off += proto::kRoomCodeLen;
  msg[off++] = (uint8_t)ufrag.size();
  std::memcpy(msg.data() + off, ufrag.data(), ufrag.size());
  off += ufrag.size();
  msg[off++] = (uint8_t)pwd.size();
  std::memcpy(msg.data() + off, pwd.data(), pwd.size());
  Send(msg.data(), msg.size());
}

void SignalingClient::SendIceCandidate(const std::string& sdp_candidate) {
  // Format: [0x08] + [6: room code] + [2: candidate_len BE] + [N: candidate]
  std::vector<uint8_t> msg(1 + proto::kRoomCodeLen + 2 + sdp_candidate.size());
  size_t off = 0;
  msg[off++] = proto::kIceCandidate;
  std::memcpy(msg.data() + off, roomCode_.data(), proto::kRoomCodeLen);
  off += proto::kRoomCodeLen;
  uint16_t cand_len = (uint16_t)sdp_candidate.size();
  msg[off++] = (uint8_t)(cand_len >> 8);
  msg[off++] = (uint8_t)(cand_len & 0xFF);
  std::memcpy(msg.data() + off, sdp_candidate.data(), sdp_candidate.size());
  Send(msg.data(), msg.size());
}

void SignalingClient::SendIceGatherDone() {
  // Format: [0x09] + [6: room code]
  uint8_t msg[1 + proto::kRoomCodeLen];
  msg[0] = proto::kIceGatherDone;
  std::memcpy(msg + 1, roomCode_.data(), proto::kRoomCodeLen);
  Send(msg, sizeof(msg));
}

void SignalingClient::SendKeepalive() {
  uint8_t msg[1 + proto::kRoomCodeLen];
  msg[0] = proto::kKeepalive;
  std::memcpy(msg + 1, roomCode_.data(), proto::kRoomCodeLen);
  Send(msg, sizeof(msg));
}

void SignalingClient::Poll() {
  if (sock_ == ENET_SOCKET_NULL) return;

  // Retry unacknowledged messages
  if ((state_ == kCreating || state_ == kJoining) && retryCount_ < kMaxRetries) {
    uint64_t elapsed = NowMs() - lastSendMs_;
    if (elapsed >= (uint64_t)kRetryIntervalMs) {
      retryCount_++;
      uint8_t msg[1 + proto::kRoomCodeLen] = {};
      msg[0] = (state_ == kCreating) ? proto::kCreateRoom : proto::kJoinRoom;
      if (state_ == kJoining) std::memcpy(msg + 1, roomCode_.data(), proto::kRoomCodeLen);
      Send(msg, sizeof(msg));
      lastSendMs_ = NowMs();
    }
  }

  // Drain all available packets from the socket
  uint8_t recv_data[2048];
  ENetBuffer recv_buf;
  recv_buf.data = recv_data;
  recv_buf.dataLength = sizeof(recv_data);

  for (;;) {
    enet_uint32 wait_condition = ENET_SOCKET_WAIT_RECEIVE;
    if (enet_socket_wait(sock_, &wait_condition, 0) != 0) break;
    if (!(wait_condition & ENET_SOCKET_WAIT_RECEIVE)) break;

    ENetAddress from_addr = {};
    int recv_len = enet_socket_receive(sock_, &from_addr, &recv_buf, 1);
    if (recv_len <= 0) break;

    HandleMessage(recv_data, (size_t)recv_len);
  }
}

void SignalingClient::HandleMessage(const uint8_t* data, size_t len) {
  if (len < 1) return;
  uint8_t type = data[0];

  switch (type) {
    case proto::kRoomCreated: {
      if (len < 1 + proto::kRoomCodeLen) break;
      roomCode_ = std::string((const char*)data + 1, proto::kRoomCodeLen);
      // Parse optional TURN credentials: [1: turn_user_len] + [N: turn_user] + [1: turn_pass_len] +
      // [N: turn_pass]
      size_t off = 1 + proto::kRoomCodeLen;
      if (off + 1 <= len) {
        uint8_t user_len = data[off++];
        if (off + user_len + 1 <= len) {
          turnUser_ = std::string((const char*)data + off, user_len);
          off += user_len;
          uint8_t pass_len = data[off++];
          if (off + pass_len <= len) {
            turnPassword_ = std::string((const char*)data + off, pass_len);
          }
        }
      }
      state_ = kHosting;
      if (on_room_created) on_room_created(roomCode_);
      break;
    }
    case proto::kPeerJoined: {
      // Parse optional TURN credentials
      size_t off = 1 + proto::kRoomCodeLen;
      if (off + 1 <= len) {
        uint8_t user_len = data[off++];
        if (off + user_len + 1 <= len) {
          turnUser_ = std::string((const char*)data + off, user_len);
          off += user_len;
          uint8_t pass_len = data[off++];
          if (off + pass_len <= len) {
            turnPassword_ = std::string((const char*)data + off, pass_len);
          }
        }
      }
      if (state_ == kJoining) {
        state_ = kWaitingForPeer;
        if (on_join_acked) on_join_acked();
      } else {
        if (on_peer_joined) on_peer_joined();
      }
      break;
    }
    case proto::kPeerAddr: {
      if (len < 1 + proto::kRoomCodeLen + 1 + 2 + 4) break;
      uint8_t addr_type = data[1 + proto::kRoomCodeLen];
      uint16_t port =
          (uint16_t)(data[1 + proto::kRoomCodeLen + 1] << 8 | data[1 + proto::kRoomCodeLen + 2]);
      int ip_len = (addr_type == proto::kAddrIPv4) ? 4 : 16;
      if ((int)len < 1 + proto::kRoomCodeLen + 3 + ip_len) break;

      char ip_str[INET6_ADDRSTRLEN] = {};
      if (addr_type == proto::kAddrIPv4)
        inet_ntop(AF_INET, data + 1 + proto::kRoomCodeLen + 3, ip_str, sizeof(ip_str));
      else
        inet_ntop(AF_INET6, data + 1 + proto::kRoomCodeLen + 3, ip_str, sizeof(ip_str));

      PeerCandidate cand{addr_type, ip_str, port};
      peerCandidates_.push_back(cand);
      if (on_peer_addr) on_peer_addr(cand);
      break;
    }
    case proto::kStartPunch: {
      state_ = kPunching;
      if (on_start_punch) on_start_punch();
      break;
    }
    case proto::kUseRelay: {
      if (len < 1 + proto::kRoomCodeLen + 2 + 8) break;
      relayPort_ =
          (uint16_t)(data[1 + proto::kRoomCodeLen] << 8 | data[1 + proto::kRoomCodeLen + 1]);
      relayToken_.assign(data + 1 + proto::kRoomCodeLen + 2,
                         data + 1 + proto::kRoomCodeLen + 2 + 8);
      state_ = kRelaying;
      if (on_use_relay) on_use_relay(relayPort_);
      break;
    }
    case proto::kRoomExpired: {
      fprintf(stderr, "[signaling] room expired\n");
      state_ = kFailed;
      if (on_room_expired) on_room_expired();
      break;
    }
    case proto::kError: {
      std::string msg;
      if (len > 2) msg = std::string((const char*)data + 2, len - 2);
      fprintf(stderr, "[signaling] ERROR from server: %s\n", msg.c_str());
      state_ = kFailed;
      if (on_error) on_error(msg);
      break;
    }
    case proto::kPeerCredentials: {
      // Format: [0x87] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
      size_t off = 1 + proto::kRoomCodeLen;
      if (off + 1 > len) break;
      uint8_t ufrag_len = data[off++];
      if (off + ufrag_len + 1 > len) break;
      std::string ufrag((const char*)data + off, ufrag_len);
      off += ufrag_len;
      uint8_t pwd_len = data[off++];
      if (off + pwd_len > len) break;
      std::string pwd((const char*)data + off, pwd_len);
      state_ = kIceExchanging;
      if (on_peer_credentials) on_peer_credentials(ufrag, pwd);
      break;
    }
    case proto::kPeerCandidate: {
      // Format: [0x88] + [6: room code] + [2: candidate_len BE] + [N: candidate]
      size_t off = 1 + proto::kRoomCodeLen;
      if (off + 2 > len) break;
      uint16_t cand_len = (uint16_t)(data[off] << 8 | data[off + 1]);
      off += 2;
      if (off + cand_len > len) break;
      std::string candidate((const char*)data + off, cand_len);
      if (on_peer_candidate) on_peer_candidate(candidate);
      break;
    }
    case proto::kPeerGatherDone: {
      if (on_peer_gather_done) on_peer_gather_done();
      break;
    }
    default:
      break;
  }
}
