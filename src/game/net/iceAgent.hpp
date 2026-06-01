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
  enum class State { New, Gathering, Connecting, Connected, Failed, Disconnected };

  struct Config {
    std::string stunServer = "stun.l.google.com";
    uint16_t stunPort = 19302;
    std::string turnServer;
    uint16_t turnPort = 3478;
    std::string turnUser;
    std::string turnPassword;
  };

  IceAgent() = default;
  ~IceAgent();

  IceAgent(const IceAgent&) = delete;
  IceAgent& operator=(const IceAgent&) = delete;

  void start(const Config& config);
  void stop();

  std::string localUfrag() const;
  std::string localPwd() const;

  void setRemoteCredentials(const std::string& ufrag, const std::string& pwd);
  void addRemoteCandidate(const std::string& candidate);
  void setRemoteGatheringDone();

  State state() const;

  // Drain the internal event queue. Call once per frame from the main thread.
  // Fires onStateChange, onLocalCandidate, onGatheringDone callbacks.
  void poll();

  // Send application data through the ICE connection.
  void send(const uint8_t* data, size_t len);

  // Callbacks — fired from poll() on the main thread.
  std::function<void(State)> onStateChange;
  std::function<void(const std::string& candidate)> onLocalCandidate;
  std::function<void()> onGatheringDone;

  // Called from libjuice's thread when data arrives. Users of IceAgent
  // should set this to write received data to the bridge socket.
  std::function<void(const uint8_t* data, size_t len)> onRecv;

 private:
  struct Event {
    enum Type { StateChanged, Candidate, GatheringDone };
    Type type;
    State newState{};
    std::string candidate;
  };

  static void cbStateChanged(juice_agent_t* agent, juice_state_t state, void* user_ptr);
  static void cbCandidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
  static void cbGatheringDone(juice_agent_t* agent, void* user_ptr);
  static void cbRecv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

  static State mapState(juice_state_t s);

  juice_agent_t* agent_ = nullptr;
  State state_ = State::New;
  mutable std::mutex mutex_;
  std::vector<Event> pendingEvents_;
};
