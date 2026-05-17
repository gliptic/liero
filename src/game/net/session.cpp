#include "session.hpp"

#include <cstring>
#include <ctime>
#include <miniz.h>

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
    , playerInfoReceived_(false)
    , matchSettingsReceived_(false)
    , mapDataReceived_(false)
{
  std::memset(&remotePlayerInfo_, 0, sizeof(remotePlayerInfo_));
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
  transport_.disconnect();
  wireCallbacks();
  sessionState_ = Idle;
  controller_.reset();
  controllerPtr_ = nullptr;
  handshakeReceived_ = false;
  handshakeSent_ = false;
  playerInfoReceived_ = false;
  matchSettingsReceived_ = false;
  mapDataReceived_ = false;
  receivedMapData_.clear();
}

void NetSession::onConnected() {
  sessionState_ = Handshaking;

  // Both sides send their handshake.
  // Host includes the seed; client sends 0 (host's seed is authoritative).
  uint32_t seedToSend = (role_ == Host) ? gameSeed_ : 0;
  transport_.sendHandshake(seedToSend, localSettingsHash_);
  handshakeSent_ = true;

  // Send local player's info from the network player profile
  auto& netWs = settings_->wormSettings[Settings::NetworkPlayerIdx];
  NetTransport::PlayerInfo info;
  for (int i = 0; i < 5; ++i)
    info.weapons[i] = netWs->weapons[i];
  info.color = netWs->color;
  for (int i = 0; i < 3; ++i)
    info.rgb[i] = netWs->rgb[i];
  transport_.sendPlayerInfo(info);

  // Host sends authoritative match settings
  if (role_ == Host) {
    NetTransport::MatchSettingsData msd;
    msd.lives = settings_->lives;
    msd.loadingTime = settings_->loadingTime;
    msd.gameMode = settings_->gameMode;
    msd.blood = settings_->blood;
    msd.maxBonuses = settings_->maxBonuses;
    msd.timeToLose = settings_->timeToLose;
    msd.flagsToWin = settings_->flagsToWin;
    msd.loadChange = settings_->loadChange ? 1 : 0;
    for (int i = 0; i < 40; ++i)
      msd.weapTable[i] = settings_->weapTable[i];
    transport_.sendMatchSettings(msd);
    matchSettingsReceived_ = true;  // Host already has correct settings
    mapDataReceived_ = true;        // Host generates locally
  }

  // Check if we can start (all data received)
  if (handshakeReceived_ && playerInfoReceived_ && matchSettingsReceived_ &&
      mapDataReceived_)
    tryStartGame();
}

void NetSession::onDisconnected() {
  sessionState_ = Disconnected;
}

void NetSession::onHandshake(uint32_t seed, uint32_t settingsHash) {
  // Client uses the host's seed
  if (role_ == Client) {
    gameSeed_ = seed;
  }

  handshakeReceived_ = true;

  if (handshakeSent_ && playerInfoReceived_ && matchSettingsReceived_ &&
      mapDataReceived_)
    tryStartGame();
}

void NetSession::onRemoteInput(uint32_t frame, uint8_t input) {
  if (controllerPtr_)
    controllerPtr_->injectRemoteInput(frame, input);
}

void NetSession::onPlayerInfo(const NetTransport::PlayerInfo& info) {
  remotePlayerInfo_ = info;
  playerInfoReceived_ = true;

  if (handshakeSent_ && handshakeReceived_ && matchSettingsReceived_ &&
      mapDataReceived_)
    tryStartGame();
}

void NetSession::onMatchSettings(const NetTransport::MatchSettingsData& data) {
  // Client applies host's authoritative settings
  if (role_ == Client) {
    settings_->lives = data.lives;
    settings_->loadingTime = data.loadingTime;
    settings_->gameMode = data.gameMode;
    settings_->blood = data.blood;
    settings_->maxBonuses = data.maxBonuses;
    settings_->timeToLose = data.timeToLose;
    settings_->flagsToWin = data.flagsToWin;
    settings_->loadChange = data.loadChange != 0;
    for (int i = 0; i < 40; ++i)
      settings_->weapTable[i] = data.weapTable[i];
  }

  matchSettingsReceived_ = true;

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ &&
      mapDataReceived_)
    tryStartGame();
}

void NetSession::onMapData(const void* data, size_t len) {
  if (role_ != Client)
    return;

  // Reject oversized map data to prevent memory exhaustion
  static constexpr size_t MAX_MAP_DATA = 10 * 1024 * 1024;  // 10 MB
  if (len > MAX_MAP_DATA)
    return;

  receivedMapData_.assign(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + len);
  mapDataReceived_ = true;

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ &&
      matchSettingsReceived_)
    tryStartGame();
}

void NetSession::onPause() {
  if (controllerPtr_)
    controllerPtr_->setRemotePaused(true);
}

void NetSession::onResume() {
  if (controllerPtr_)
    controllerPtr_->setRemotePaused(false);
}

void NetSession::sendPause() {
  transport_.sendPause();
}

void NetSession::sendResume() {
  transport_.sendResume();
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
  transport_.onPlayerInfo = [this](const NetTransport::PlayerInfo& info) {
    onPlayerInfo(info);
  };
  transport_.onMatchSettings = [this](const NetTransport::MatchSettingsData& data) {
    onMatchSettings(data);
  };
  transport_.onMapData = [this](const void* data, size_t len) {
    onMapData(data, len);
  };
  transport_.onPause = [this]() { onPause(); };
  transport_.onResume = [this]() { onResume(); };
}

void NetSession::tryStartGame() {
  if (sessionState_ == Playing)
    return;

  // Apply remote player's info to the remote worm directly (not the persistent settings)
  int remoteIdx = (role_ == Host) ? 1 : 0;
  Worm* remoteWorm = controller_->game.wormByIdx(remoteIdx);
  // Create a distinct copy so we don't mutate the saved player profile
  auto remoteWs = std::make_shared<WormSettings>(*remoteWorm->settings);
  for (int i = 0; i < 5; ++i)
    remoteWs->weapons[i] = remotePlayerInfo_.weapons[i];
  remoteWs->color = remotePlayerInfo_.color;
  for (int i = 0; i < 3; ++i)
    remoteWs->rgb[i] = remotePlayerInfo_.rgb[i];
  remoteWorm->settings = remoteWs;

  // Seed the game's RNG so both peers have identical state
  controller_->game.rand.seed(gameSeed_);

  if (role_ == Host) {
    // Host generates the level and sends it to the client
    generateAndSendMap();
    controller_->setLevelPreloaded();
  } else {
    // Client loads the level from received compressed map data
    controller_->loadLevelFromData(receivedMapData_);
    receivedMapData_.clear();
  }

  // Wire the controller to send inputs via transport
  controller_->setInputCallbacks(
      [this](uint32_t frame, uint8_t input) {
        transport_.sendInput(frame, input);
      },
      nullptr  // We use injectRemoteInput via onRemoteInput callback instead
  );

  // Wire pause/resume callbacks
  controller_->setPauseCallbacks(
      [this]() { transport_.sendPause(); },
      [this]() { transport_.sendResume(); });

  // Pre-fill the input delay window with empty inputs so both sides
  // can advance past the initial frames without stalling.
  for (uint32_t i = 0; i < 3; ++i) {
    controller_->injectRemoteInput(i, 0);
  }

  controllerPtr_ = controller_.get();
  sessionState_ = Playing;
}

void NetSession::generateAndSendMap() {
  // Generate the level on the host
  controller_->game.level.generateFromSettings(
      *controller_->game.common, *controller_->game.settings,
      controller_->game.rand);

  Level& level = controller_->game.level;

  // Serialize: width(2) + height(2) + rand_x(4) + rand_c(4) + pixel_data(w*h) + palette(768)
  uint16_t w = static_cast<uint16_t>(level.width);
  uint16_t h = static_cast<uint16_t>(level.height);
  uint32_t randX = controller_->game.rand.x;
  uint32_t randC = controller_->game.rand.c;
  size_t pixelDataSize = static_cast<size_t>(w) * h;
  size_t rawSize = 4 + 8 + pixelDataSize + 768;

  std::vector<uint8_t> raw(rawSize);
  std::memcpy(raw.data(), &w, 2);
  std::memcpy(raw.data() + 2, &h, 2);
  std::memcpy(raw.data() + 4, &randX, 4);
  std::memcpy(raw.data() + 8, &randC, 4);
  std::memcpy(raw.data() + 12, level.data.data(), pixelDataSize);

  // Palette
  uint8_t* palPtr = raw.data() + 12 + pixelDataSize;
  for (int i = 0; i < 256; ++i) {
    palPtr[i * 3 + 0] = level.origpal.entries[i].r;
    palPtr[i * 3 + 1] = level.origpal.entries[i].g;
    palPtr[i * 3 + 2] = level.origpal.entries[i].b;
  }

  // Compress with miniz
  mz_ulong compBound = mz_compressBound(static_cast<mz_ulong>(rawSize));
  std::vector<uint8_t> compressed(compBound);
  mz_ulong compSize = compBound;
  int status = mz_compress(compressed.data(), &compSize, raw.data(),
                           static_cast<mz_ulong>(rawSize));
  if (status == MZ_OK) {
    compressed.resize(compSize);
  } else {
    // Fallback: send uncompressed
    compressed = std::move(raw);
  }

  // Send: compressed flag(1) + uncompressedSize(4) + data
  std::vector<uint8_t> packet(1 + 4 + compressed.size());
  packet[0] = (status == MZ_OK) ? 1 : 0;
  uint32_t rawSize32 = static_cast<uint32_t>(rawSize);
  std::memcpy(packet.data() + 1, &rawSize32, 4);
  std::memcpy(packet.data() + 5, compressed.data(), compressed.size());

  transport_.sendMapData(packet.data(), packet.size());
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
