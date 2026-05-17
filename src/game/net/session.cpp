#include "session.hpp"

#include <ctime>

NetSession::NetSession(std::shared_ptr<Common> common,
                       std::shared_ptr<Settings> settings)
    : role_(Host)
    , sessionState_(Idle)
    , common_(std::move(common))
    , settings_(std::move(settings))
    , controllerPtr_(nullptr)
    , gameSeed_(0)
    , localSettingsHash_(0)
    , handshakeReceived_(false)
    , handshakeSent_(false)
{
  localSettingsHash_ = computeSettingsHash();
  wireCallbacks();
}

NetSession::~NetSession() {
  disconnect();
}

bool NetSession::hostGame(uint16_t port) {
  if (sessionState_ != Idle)
    return false;

  role_ = Host;
  gameSeed_ = static_cast<uint32_t>(std::time(nullptr));

  // Create controller: host is player 0
  controller_ = std::make_unique<NetworkController>(common_, settings_, 0);

  if (!transport_.host(port)) {
    sessionState_ = Failed;
    return false;
  }
  sessionState_ = WaitingForPeer;
  return true;
}

bool NetSession::joinGame(const std::string& address, uint16_t port) {
  if (sessionState_ != Idle)
    return false;

  role_ = Client;
  gameSeed_ = 0;  // Will be set by host's handshake

  // Create controller: client is player 1
  controller_ = std::make_unique<NetworkController>(common_, settings_, 1);

  if (!transport_.connect(address, port)) {
    sessionState_ = Failed;
    return false;
  }
  sessionState_ = WaitingForPeer;
  return true;
}

void NetSession::update() {
  if (sessionState_ == Idle || sessionState_ == Failed ||
      sessionState_ == Disconnected)
    return;

  transport_.poll();

  if (transport_.state() == NetTransport::Failed) {
    sessionState_ = Failed;
    return;
  }

  if (transport_.state() == NetTransport::Disconnected &&
      sessionState_ != Idle) {
    sessionState_ = Disconnected;
    return;
  }
}

void NetSession::disconnect() {
  transport_.~NetTransport();
  new (&transport_) NetTransport();
  wireCallbacks();
  sessionState_ = Idle;
  controller_.reset();
  controllerPtr_ = nullptr;
  handshakeReceived_ = false;
  handshakeSent_ = false;
}

void NetSession::onConnected() {
  sessionState_ = Handshaking;

  // Both sides send their handshake.
  // Host includes the seed; client sends 0 (host's seed is authoritative).
  uint32_t seedToSend = (role_ == Host) ? gameSeed_ : 0;
  transport_.sendHandshake(seedToSend, localSettingsHash_);
  handshakeSent_ = true;

  // If we somehow already received the handshake (unlikely), try to start
  if (handshakeReceived_)
    startGame();
}

void NetSession::onDisconnected() {
  sessionState_ = Disconnected;
}

void NetSession::onHandshake(uint32_t seed, uint32_t settingsHash) {
  if (settingsHash != localSettingsHash_) {
    // Settings mismatch — cannot play together
    sessionState_ = Failed;
    return;
  }

  // Client uses the host's seed
  if (role_ == Client) {
    gameSeed_ = seed;
  }

  handshakeReceived_ = true;

  if (handshakeSent_)
    startGame();
}

void NetSession::onRemoteInput(uint32_t frame, uint8_t input) {
  if (controllerPtr_)
    controllerPtr_->injectRemoteInput(frame, input);
}

void NetSession::wireCallbacks() {
  transport_.onConnected = [this]() { onConnected(); };
  transport_.onDisconnected = [this]() { onDisconnected(); };
  transport_.onHandshake = [this](uint32_t seed, uint32_t hash) {
    onHandshake(seed, hash);
  };
  transport_.onRemoteInput = [this](uint32_t frame, uint8_t input) {
    onRemoteInput(frame, input);
  };
}

void NetSession::startGame() {
  if (sessionState_ == Playing)
    return;

  // Seed the game's RNG so both peers have identical state
  controller_->game.rand.seed(gameSeed_);

  // Wire the controller to send inputs via transport
  controller_->setInputCallbacks(
      [this](uint32_t frame, uint8_t input) {
        transport_.sendInput(frame, input);
      },
      nullptr  // We use injectRemoteInput via onRemoteInput callback instead
  );

  // Pre-fill the input delay window with empty inputs so both sides
  // can advance past the initial frames without stalling.
  for (uint32_t i = 0; i < 3; ++i) {
    controller_->injectRemoteInput(i, 0);
  }

  controllerPtr_ = controller_.get();
  sessionState_ = Playing;
}

std::unique_ptr<NetworkController> NetSession::releaseController() {
  return std::move(controller_);
}

uint32_t NetSession::computeSettingsHash() const {
  // Hash the gameplay-relevant settings to detect mismatches.
  // Uses a simple FNV-1a hash over the key fields.
  uint32_t hash = 2166136261u;
  auto mix = [&hash](uint32_t val) {
    for (int i = 0; i < 4; ++i) {
      hash ^= (val >> (i * 8)) & 0xFF;
      hash *= 16777619u;
    }
  };

  mix(settings_->lives);
  mix(settings_->loadChange ? 1u : 0u);
  mix(settings_->maxBonuses);
  mix(settings_->blood);
  mix(settings_->timeToLose);
  mix(settings_->flagsToWin);
  mix(static_cast<uint32_t>(settings_->gameMode));
  mix(settings_->loadingTime);

  return hash;
}
