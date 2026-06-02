#include "iceAgent.hpp"

#include <cstring>

IceAgent::~IceAgent() { Stop(); }

void IceAgent::Start(const Config& config) {
  if (agent_) return;

  juice_config_t jcfg{};
  jcfg.concurrency_mode = JUICE_CONCURRENCY_MODE_THREAD;

  juice_turn_server_t turn{};
  if (!config.turn_server.empty()) {
    turn.host = config.turn_server.c_str();
    turn.port = config.turn_port;
    turn.username = config.turn_user.c_str();
    turn.password = config.turn_password.c_str();
    jcfg.turn_servers = &turn;
    jcfg.turn_servers_count = 1;
  }

  jcfg.stun_server_host = config.stun_server.c_str();
  jcfg.stun_server_port = config.stun_port;

  jcfg.cb_state_changed = CbStateChanged;
  jcfg.cb_candidate = CbCandidate;
  jcfg.cb_gathering_done = CbGatheringDone;
  jcfg.cb_recv = CbRecv;
  jcfg.user_ptr = this;

  agent_ = juice_create(&jcfg);
  juice_gather_candidates(agent_);
}

void IceAgent::Stop() {
  if (!agent_) return;
  juice_destroy(agent_);
  agent_ = nullptr;
  state_ = State::kDisconnected;
}

std::string IceAgent::LocalUfrag() const {
  if (!agent_) return {};
  char buf[256];
  if (juice_get_local_description(agent_, buf, sizeof(buf)) == JUICE_ERR_SUCCESS) {
    // Format: "a=ice-ufrag:<ufrag>\r\na=ice-pwd:<pwd>\r\n"
    // Extract ufrag
    const char* ufrag_start = std::strstr(buf, "a=ice-ufrag:");
    if (!ufrag_start) return {};
    ufrag_start += 12;
    const char* ufrag_end = std::strstr(ufrag_start, "\r\n");
    if (!ufrag_end) ufrag_end = ufrag_start + std::strlen(ufrag_start);
    return std::string(ufrag_start, ufrag_end);
  }
  return {};
}

std::string IceAgent::LocalPwd() const {
  if (!agent_) return {};
  char buf[256];
  if (juice_get_local_description(agent_, buf, sizeof(buf)) == JUICE_ERR_SUCCESS) {
    const char* pwd_start = std::strstr(buf, "a=ice-pwd:");
    if (!pwd_start) return {};
    pwd_start += 10;
    const char* pwd_end = std::strstr(pwd_start, "\r\n");
    if (!pwd_end) pwd_end = pwd_start + std::strlen(pwd_start);
    return std::string(pwd_start, pwd_end);
  }
  return {};
}

void IceAgent::SetRemoteCredentials(const std::string& ufrag, const std::string& pwd) {
  if (!agent_) return;
  std::string desc = "a=ice-ufrag:" + ufrag + "\r\na=ice-pwd:" + pwd + "\r\n";
  juice_set_remote_description(agent_, desc.c_str());
}

void IceAgent::AddRemoteCandidate(const std::string& candidate) {
  if (!agent_) return;
  juice_add_remote_candidate(agent_, candidate.c_str());
}

void IceAgent::SetRemoteGatheringDone() {
  if (!agent_) return;
  juice_set_remote_gathering_done(agent_);
}

IceAgent::State IceAgent::CurrentState() const { return state_; }

void IceAgent::Poll() {
  std::vector<Event> events;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    events.swap(pendingEvents_);
  }
  for (auto& ev : events) {
    switch (ev.type) {
      case Event::kStateChanged:
        state_ = ev.new_state;
        if (on_state_change) on_state_change(ev.new_state);
        break;
      case Event::kCandidate:
        if (on_local_candidate) on_local_candidate(ev.candidate);
        break;
      case Event::kGatheringDone:
        if (on_gathering_done) on_gathering_done();
        break;
    }
  }
}

void IceAgent::Send(const uint8_t* data, size_t len) {
  if (!agent_) return;
  juice_send(agent_, reinterpret_cast<const char*>(data), len);
}

// --- Static callbacks (called from libjuice's internal thread) ---

void IceAgent::CbStateChanged(juice_agent_t*, juice_state_t state, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  Event ev;
  ev.type = Event::kStateChanged;
  ev.new_state = MapState(state);
  std::lock_guard<std::mutex> lock(self->mutex_);
  self->pendingEvents_.push_back(std::move(ev));
}

void IceAgent::CbCandidate(juice_agent_t*, const char* sdp, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  Event ev;
  ev.type = Event::kCandidate;
  ev.candidate = sdp;
  std::lock_guard<std::mutex> lock(self->mutex_);
  self->pendingEvents_.push_back(std::move(ev));
}

void IceAgent::CbGatheringDone(juice_agent_t*, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  Event ev;
  ev.type = Event::kGatheringDone;
  std::lock_guard<std::mutex> lock(self->mutex_);
  self->pendingEvents_.push_back(std::move(ev));
}

void IceAgent::CbRecv(juice_agent_t*, const char* data, size_t size, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  if (self->on_recv) {
    self->on_recv(reinterpret_cast<const uint8_t*>(data), size);
  }
}

IceAgent::State IceAgent::MapState(juice_state_t s) {
  switch (s) {
    case JUICE_STATE_DISCONNECTED:
      return State::kDisconnected;
    case JUICE_STATE_GATHERING:
      return State::kGathering;
    case JUICE_STATE_CONNECTING:
      return State::kConnecting;
    case JUICE_STATE_CONNECTED:
      return State::kConnected;
    case JUICE_STATE_COMPLETED:
      return State::kConnected;
    case JUICE_STATE_FAILED:
      return State::kFailed;
    default:
      return State::kNew;
  }
}
