#include "iceAgent.hpp"

#include <cstring>

IceAgent::~IceAgent() { stop(); }

void IceAgent::start(const Config& config) {
  if (agent_) return;

  juice_config_t jcfg{};
  jcfg.concurrency_mode = JUICE_CONCURRENCY_MODE_THREAD;

  juice_turn_server_t turn{};
  if (!config.turnServer.empty()) {
    turn.host = config.turnServer.c_str();
    turn.port = config.turnPort;
    turn.username = config.turnUser.c_str();
    turn.password = config.turnPassword.c_str();
    jcfg.turn_servers = &turn;
    jcfg.turn_servers_count = 1;
  }

  jcfg.stun_server_host = config.stunServer.c_str();
  jcfg.stun_server_port = config.stunPort;

  jcfg.cb_state_changed = cbStateChanged;
  jcfg.cb_candidate = cbCandidate;
  jcfg.cb_gathering_done = cbGatheringDone;
  jcfg.cb_recv = cbRecv;
  jcfg.user_ptr = this;

  agent_ = juice_create(&jcfg);
  juice_gather_candidates(agent_);
}

void IceAgent::stop() {
  if (!agent_) return;
  juice_destroy(agent_);
  agent_ = nullptr;
  state_ = State::Disconnected;
}

std::string IceAgent::localUfrag() const {
  if (!agent_) return {};
  char buf[256];
  if (juice_get_local_description(agent_, buf, sizeof(buf)) == JUICE_ERR_SUCCESS) {
    // Format: "a=ice-ufrag:<ufrag>\r\na=ice-pwd:<pwd>\r\n"
    // Extract ufrag
    const char* ufragStart = std::strstr(buf, "a=ice-ufrag:");
    if (!ufragStart) return {};
    ufragStart += 12;
    const char* ufragEnd = std::strstr(ufragStart, "\r\n");
    if (!ufragEnd) ufragEnd = ufragStart + std::strlen(ufragStart);
    return std::string(ufragStart, ufragEnd);
  }
  return {};
}

std::string IceAgent::localPwd() const {
  if (!agent_) return {};
  char buf[256];
  if (juice_get_local_description(agent_, buf, sizeof(buf)) == JUICE_ERR_SUCCESS) {
    const char* pwdStart = std::strstr(buf, "a=ice-pwd:");
    if (!pwdStart) return {};
    pwdStart += 10;
    const char* pwdEnd = std::strstr(pwdStart, "\r\n");
    if (!pwdEnd) pwdEnd = pwdStart + std::strlen(pwdStart);
    return std::string(pwdStart, pwdEnd);
  }
  return {};
}

void IceAgent::setRemoteCredentials(const std::string& ufrag, const std::string& pwd) {
  if (!agent_) return;
  std::string desc = "a=ice-ufrag:" + ufrag + "\r\na=ice-pwd:" + pwd + "\r\n";
  juice_set_remote_description(agent_, desc.c_str());
}

void IceAgent::addRemoteCandidate(const std::string& candidate) {
  if (!agent_) return;
  juice_add_remote_candidate(agent_, candidate.c_str());
}

void IceAgent::setRemoteGatheringDone() {
  if (!agent_) return;
  juice_set_remote_gathering_done(agent_);
}

IceAgent::State IceAgent::state() const { return state_; }

void IceAgent::poll() {
  std::vector<Event> events;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    events.swap(pendingEvents_);
  }
  for (auto& ev : events) {
    switch (ev.type) {
      case Event::StateChanged:
        state_ = ev.newState;
        if (onStateChange) onStateChange(ev.newState);
        break;
      case Event::Candidate:
        if (onLocalCandidate) onLocalCandidate(ev.candidate);
        break;
      case Event::GatheringDone:
        if (onGatheringDone) onGatheringDone();
        break;
    }
  }
}

void IceAgent::send(const uint8_t* data, size_t len) {
  if (!agent_) return;
  juice_send(agent_, reinterpret_cast<const char*>(data), len);
}

// --- Static callbacks (called from libjuice's internal thread) ---

void IceAgent::cbStateChanged(juice_agent_t*, juice_state_t state, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  Event ev;
  ev.type = Event::StateChanged;
  ev.newState = mapState(state);
  std::lock_guard<std::mutex> lock(self->mutex_);
  self->pendingEvents_.push_back(std::move(ev));
}

void IceAgent::cbCandidate(juice_agent_t*, const char* sdp, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  Event ev;
  ev.type = Event::Candidate;
  ev.candidate = sdp;
  std::lock_guard<std::mutex> lock(self->mutex_);
  self->pendingEvents_.push_back(std::move(ev));
}

void IceAgent::cbGatheringDone(juice_agent_t*, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  Event ev;
  ev.type = Event::GatheringDone;
  std::lock_guard<std::mutex> lock(self->mutex_);
  self->pendingEvents_.push_back(std::move(ev));
}

void IceAgent::cbRecv(juice_agent_t*, const char* data, size_t size, void* user_ptr) {
  auto* self = static_cast<IceAgent*>(user_ptr);
  if (self->onRecv) {
    self->onRecv(reinterpret_cast<const uint8_t*>(data), size);
  }
}

IceAgent::State IceAgent::mapState(juice_state_t s) {
  switch (s) {
    case JUICE_STATE_DISCONNECTED: return State::Disconnected;
    case JUICE_STATE_GATHERING: return State::Gathering;
    case JUICE_STATE_CONNECTING: return State::Connecting;
    case JUICE_STATE_CONNECTED: return State::Connected;
    case JUICE_STATE_COMPLETED: return State::Connected;
    case JUICE_STATE_FAILED: return State::Failed;
    default: return State::New;
  }
}
