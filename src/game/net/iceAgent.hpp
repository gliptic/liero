#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <juice/juice.h>

// C++ wrapper around libjuice providing ICE connectivity with a thread-safe
// event queue. libjuice callbacks fire from an internal thread; IceAgent
// buffers events and delivers them on the main thread via poll().
struct IceAgent {
  enum class State { kNew, kGathering, kConnecting, kConnected, kFailed, kDisconnected };

  struct Config {
    std::string stun_server = "stun.l.google.com";
    uint16_t stun_port = 19302;
    std::string turn_server;
    uint16_t turn_port = 3478;
    std::string turn_user;
    std::string turn_password;
  };

  IceAgent() = default;
  ~IceAgent();

  IceAgent(const IceAgent&) = delete;
  IceAgent& operator=(const IceAgent&) = delete;

  void Start(const Config& config);
  void Stop();

  std::string LocalUfrag() const;
  std::string LocalPwd() const;

  void SetRemoteCredentials(const std::string& ufrag, const std::string& pwd);
  void AddRemoteCandidate(const std::string& candidate);
  void SetRemoteGatheringDone();

  State CurrentState() const;

  // Drain the internal event queue. Call once per frame from the main thread.
  // Fires onStateChange, onLocalCandidate, onGatheringDone callbacks.
  void Poll();

  // Send application data through the ICE connection.
  void Send(const uint8_t* data, size_t len);

  // Callbacks — fired from poll() on the main thread.
  std::function<void(State)> on_state_change;
  std::function<void(const std::string& candidate)> on_local_candidate;
  std::function<void()> on_gathering_done;

  // Called from libjuice's thread when data arrives. Users of IceAgent
  // should set this to write received data to the bridge socket.
  std::function<void(const uint8_t* data, size_t len)> on_recv;

 private:
  struct Event {
    enum Type { kStateChanged, kCandidate, kGatheringDone };
    Type type;
    enum State new_state {};
    std::string candidate;
  };

  static void CbStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
  static void CbCandidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
  static void CbGatheringDone(juice_agent_t* agent, void* user_ptr);
  static void CbRecv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

  static enum State MapState(juice_state_t s);

  juice_agent_t* agent_ = nullptr;
  enum State state_ = State::kNew;
  mutable std::mutex mutex_;
  std::vector<Event> pendingEvents_;
};
