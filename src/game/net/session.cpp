#include "session.hpp"

#include <cstring>
#include <ctime>
#include <miniz.h>

#include "memoryFs.hpp"
#include "tcArchive.hpp"

NetSession::NetSession(std::shared_ptr<Common> common,
                       std::shared_ptr<Settings> settings,
                       FsNode tcRoot)
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
    , localReady_(false)
    , remoteReady_(false)
    , tcRoot_(std::move(tcRoot))
    , localTcHash_(0)
    , tcResolved_(false)
    , originalTcName_(settings_->tc)
    , originalCommon_(common_)
    , desyncDetected_(false)
    , desyncFrame_(0)
{
  std::memset(&remotePlayerInfo_, 0, sizeof(remotePlayerInfo_));
  localSettingsHash_ = computeSettingsHash();
  localTcHash_ = TcArchive::computeHash(tcRoot_);
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

bool NetSession::hostWithTransport(NetTransport&& transport) {
  if (sessionState_ != Idle)
    return false;

  role_ = Host;
  gameSeed_ = static_cast<uint32_t>(std::time(nullptr));
  controller_ = std::make_unique<NetworkController>(common_, settings_, 0);

  transport_ = std::move(transport);
  wireCallbacks();
  sessionState_ = WaitingForPeer;
  return true;
}

bool NetSession::connectWithTransport(NetTransport&& transport,
                                      const std::string& peerAddr, uint16_t peerPort) {
  if (sessionState_ != Idle)
    return false;

  role_ = Client;
  gameSeed_ = 0;
  controller_ = std::make_unique<NetworkController>(common_, settings_, 1);

  transport_ = std::move(transport);
  wireCallbacks();

  if (!transport_.connectExisting(peerAddr, peerPort)) {
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
  localReady_ = false;
  remoteReady_ = false;
  receivedMapData_.clear();

  // Restore client's original TC if it was changed during the session
  if (role_ == Client && tcMemFs_) {
    settings_->tc = originalTcName_;
    if (onTcReloaded)
      onTcReloaded(originalCommon_);
    common_ = originalCommon_;
    tcMemFs_.reset();
  }
}

void NetSession::onConnected() {
  sessionState_ = Handshaking;

  // Host sends TC info first so client can verify/request TC data
  if (role_ == Host) {
    transport_.sendTcInfo(localTcHash_, settings_->tc);
    tcResolved_ = true;  // Host always has correct TC
  }

  // Both sides send their handshake.
  // Host includes the seed; client sends 0 (host's seed is authoritative).
  uint32_t seedToSend = (role_ == Host) ? gameSeed_ : 0;
  transport_.sendHandshake(seedToSend, localSettingsHash_);
  handshakeSent_ = true;

  // Send local player's info from the network player profile
  auto& netWs = settings_->wormSettings[Settings::NetworkPlayerIdx];
  NetTransport::PlayerInfo info{};
  for (int i = 0; i < 5; ++i)
    info.weapons[i] = netWs->weapons[i];
  info.color = netWs->color;
  for (int i = 0; i < 3; ++i)
    info.rgb[i] = netWs->rgb[i];
  std::strncpy(info.name, netWs->name.c_str(), sizeof(info.name) - 1);
  info.name[sizeof(info.name) - 1] = '\0';
  transport_.sendPlayerInfo(info);

  // Host sends authoritative match settings
  if (role_ == Host) {
    NetTransport::MatchSettingsData msd{};
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
    msd.regenerateLevel = settings_->regenerateLevel ? 1 : 0;
    msd.shadow = settings_->shadow ? 1 : 0;
    msd.namesOnBonuses = settings_->namesOnBonuses ? 1 : 0;
    msd.bloodParticleMax = settings_->bloodParticleMax;
    msd.zoneTimeout = settings_->zoneTimeout;
    transport_.sendMatchSettings(msd);
    matchSettingsReceived_ = true;  // Host already has correct settings
    mapDataReceived_ = true;        // Host generates locally
  }

  // Check if we can start (all data received)
  if (handshakeReceived_ && playerInfoReceived_ && matchSettingsReceived_ &&
      mapDataReceived_ && tcResolved_)
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

  if (sessionState_ == Rematch) {
    // During rematch, handshake signals the host is starting the game.
    // Client needs to wait for map data before creating the controller.
    handshakeReceived_ = true;
    return;
  }

  handshakeReceived_ = true;

  if (handshakeSent_ && playerInfoReceived_ && matchSettingsReceived_ &&
      mapDataReceived_ && tcResolved_)
    tryStartGame();
}

void NetSession::onRemoteInput(uint32_t frame, uint8_t input) {
  if (controllerPtr_)
    controllerPtr_->injectRemoteInput(frame, input);
  else
    pendingInputs_.push_back({frame, input});
}

void NetSession::onPlayerInfo(const NetTransport::PlayerInfo& info) {
  remotePlayerInfo_ = info;
  playerInfoReceived_ = true;

  if (handshakeSent_ && handshakeReceived_ && matchSettingsReceived_ &&
      mapDataReceived_ && tcResolved_)
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
    settings_->regenerateLevel = data.regenerateLevel != 0;
    settings_->shadow = data.shadow != 0;
    settings_->namesOnBonuses = data.namesOnBonuses != 0;
    settings_->bloodParticleMax = data.bloodParticleMax;
    settings_->zoneTimeout = data.zoneTimeout;
  }

  matchSettingsReceived_ = true;

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ &&
      mapDataReceived_ && tcResolved_)
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

  if (sessionState_ == Rematch && handshakeReceived_) {
    // We have the seed and map — start the client-side rematch
    startRematchClient();
    return;
  }

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ &&
      matchSettingsReceived_ && tcResolved_)
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

void NetSession::onRemoteEndMatch() {
  if (controllerPtr_)
    controllerPtr_->endMatch();
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
  transport_.onEndMatch = [this]() { onRemoteEndMatch(); };
  transport_.onChecksum = [this](uint32_t frame, uint32_t checksum) {
    onChecksum(frame, checksum);
  };
  transport_.onRematchReady = [this](bool ready) { onRematchReady(ready); };
  transport_.onRematchLevel = [this](bool random, std::string file) {
    onRematchLevel(random, std::move(file));
  };
  transport_.onTcInfo = [this](uint32_t hash, std::string name) {
    onTcInfo(hash, std::move(name));
  };
  transport_.onTcResponse = [this](bool needData) {
    onTcResponse(needData);
  };
  transport_.onTcData = [this](const void* data, size_t len) {
    onTcData(data, len);
  };
}

void NetSession::onTcInfo(uint32_t hash, std::string name) {
  // Client receives TC info from host
  if (role_ != Client) return;

  if (name == settings_->tc && hash == localTcHash_) {
    // Same TC, no transfer needed
    transport_.sendTcResponse(false);
    tcResolved_ = true;

    if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ &&
        matchSettingsReceived_ && mapDataReceived_)
      tryStartGame();
  } else {
    // Different TC — request the data
    settings_->tc = name;
    transport_.sendTcResponse(true);
  }
}

void NetSession::onTcResponse(bool needData) {
  // Host receives client's response about TC
  if (role_ != Host) return;

  if (needData) {
    // Client needs the TC archive — pack and send it
    auto archive = TcArchive::pack(tcRoot_);
    transport_.sendTcData(archive.data(), archive.size());
  }
  // If !needData, the client already has the TC — nothing more to do
}

void NetSession::onTcData(const void* data, size_t len) {
  // Client receives TC archive from host
  if (role_ != Client) return;

  // Reject oversized TC data
  static constexpr size_t MAX_TC_DATA = 50 * 1024 * 1024;  // 50 MB
  if (len > MAX_TC_DATA) return;

  auto files = TcArchive::unpack(static_cast<const uint8_t*>(data), len);
  if (files.empty()) return;

  // Load TC from memory (no disk writes — platform-agnostic)
  auto memFs = std::make_shared<MemoryFs>();
  for (auto& file : files) {
    memFs->files[file.name] = std::move(file.data);
  }

  // Keep the MemoryFs alive by storing it in the session
  tcMemFs_ = memFs;

  // Reload Common from the in-memory TC
  auto newCommon = std::make_shared<Common>();
  newCommon->load(memFs->root());

  common_ = newCommon;
  controller_ = std::make_unique<NetworkController>(common_, settings_, 1);

  if (onTcReloaded)
    onTcReloaded(newCommon);

  tcResolved_ = true;

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ &&
      matchSettingsReceived_ && mapDataReceived_)
    tryStartGame();
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
  remoteWs->name = remotePlayerInfo_.name;
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

  // Wire checksum sending for desync detection
  controller_->setChecksumCallback(
      [this](uint32_t frame, uint32_t checksum) {
        transport_.sendChecksum(frame, checksum);
        onLocalChecksum(frame, checksum);
      });

  // Wire pause/resume callbacks
  controller_->setPauseCallbacks(
      [this]() { transport_.sendPause(); },
      [this]() { transport_.sendResume(); });
  controller_->setEndMatchCallback(
      [this]() { transport_.sendEndMatch(); });

  // Pre-fill the input delay window with empty inputs so both sides
  // can advance past the initial frames without stalling.
  for (uint32_t i = 0; i < 3; ++i) {
    controller_->injectRemoteInput(i, 0);
  }

  controllerPtr_ = controller_.get();

  // Flush any inputs that arrived before the controller was ready
  for (auto& pi : pendingInputs_) {
    controllerPtr_->injectRemoteInput(pi.frame, pi.input);
  }
  pendingInputs_.clear();

  sessionState_ = Playing;
}

void NetSession::enterRematch() {
  if (sessionState_ != Playing)
    return;

  sessionState_ = Rematch;
  localReady_ = false;
  remoteReady_ = false;

  // The old controller is still owned by Gfx. Clear our raw pointer since
  // it will be replaced when the rematch starts.
  controllerPtr_ = nullptr;

  // Reset per-game handshake flags for the next round
  mapDataReceived_ = (role_ == Host);  // host generates locally
  receivedMapData_.clear();
}

void NetSession::toggleReady() {
  if (sessionState_ != Rematch)
    return;

  localReady_ = !localReady_;
  transport_.sendRematchReady(localReady_);

  // Only host initiates the rematch start
  if (role_ == Host && localReady_ && remoteReady_)
    startRematch();
}

void NetSession::setRematchLevel(bool randomLevel, const std::string& levelFile) {
  if (sessionState_ != Rematch || role_ != Host)
    return;

  settings_->randomLevel = randomLevel;
  settings_->levelFile = levelFile;

  transport_.sendRematchLevel(randomLevel, levelFile);

  // Reset ready states when level changes
  if (localReady_) {
    localReady_ = false;
    transport_.sendRematchReady(false);
  }
  if (remoteReady_)
    remoteReady_ = false;
}

void NetSession::onRematchReady(bool ready) {
  if (sessionState_ != Rematch)
    return;

  remoteReady_ = ready;

  // Only host initiates the rematch start
  if (role_ == Host && localReady_ && remoteReady_)
    startRematch();
}

void NetSession::onRematchLevel(bool randomLevel, std::string levelFile) {
  if (sessionState_ != Rematch || role_ != Client)
    return;

  settings_->randomLevel = randomLevel;
  settings_->levelFile = std::move(levelFile);

  // Reset ready states when level changes
  localReady_ = false;
  remoteReady_ = false;
}

void NetSession::startRematch() {
  // Only the host calls this directly. The client starts via onHandshake+onMapData.
  if (sessionState_ != Rematch || role_ != Host)
    return;

  // Generate a new seed
  gameSeed_ = static_cast<uint32_t>(std::time(nullptr));

  // Create a fresh controller (host = player 0)
  controller_ = std::make_unique<NetworkController>(common_, settings_, 0);

  // Apply remote player info
  Worm* remoteWorm = controller_->game.wormByIdx(1);
  auto remoteWs = std::make_shared<WormSettings>(*remoteWorm->settings);
  for (int i = 0; i < 5; ++i)
    remoteWs->weapons[i] = remotePlayerInfo_.weapons[i];
  remoteWs->color = remotePlayerInfo_.color;
  for (int i = 0; i < 3; ++i)
    remoteWs->rgb[i] = remotePlayerInfo_.rgb[i];
  remoteWs->name = remotePlayerInfo_.name;
  remoteWorm->settings = remoteWs;

  controller_->game.rand.seed(gameSeed_);

  // Send seed to client, then generate and send map
  transport_.sendHandshake(gameSeed_, 0);
  generateAndSendMap();
  controller_->setLevelPreloaded();

  // Wire controller
  controller_->setInputCallbacks(
      [this](uint32_t frame, uint8_t input) {
        transport_.sendInput(frame, input);
      },
      nullptr);
  controller_->setChecksumCallback(
      [this](uint32_t frame, uint32_t checksum) {
        transport_.sendChecksum(frame, checksum);
        onLocalChecksum(frame, checksum);
      });
  controller_->setPauseCallbacks(
      [this]() { transport_.sendPause(); },
      [this]() { transport_.sendResume(); });
  controller_->setEndMatchCallback(
      [this]() { transport_.sendEndMatch(); });
  for (uint32_t i = 0; i < 3; ++i)
    controller_->injectRemoteInput(i, 0);

  controllerPtr_ = controller_.get();

  // Flush any inputs that arrived before the controller was ready
  for (auto& pi : pendingInputs_) {
    controllerPtr_->injectRemoteInput(pi.frame, pi.input);
  }
  pendingInputs_.clear();

  localReady_ = false;
  remoteReady_ = false;
  sessionState_ = Playing;
}

void NetSession::startRematchClient() {
  // Called on the client side when handshake (seed) + map data are both received
  if (sessionState_ != Rematch || role_ != Client)
    return;

  // Create a fresh controller (client = player 1)
  controller_ = std::make_unique<NetworkController>(common_, settings_, 1);

  // Apply remote player info (host = player 0)
  Worm* remoteWorm = controller_->game.wormByIdx(0);
  auto remoteWs = std::make_shared<WormSettings>(*remoteWorm->settings);
  for (int i = 0; i < 5; ++i)
    remoteWs->weapons[i] = remotePlayerInfo_.weapons[i];
  remoteWs->color = remotePlayerInfo_.color;
  for (int i = 0; i < 3; ++i)
    remoteWs->rgb[i] = remotePlayerInfo_.rgb[i];
  remoteWs->name = remotePlayerInfo_.name;
  remoteWorm->settings = remoteWs;

  controller_->game.rand.seed(gameSeed_);
  controller_->loadLevelFromData(receivedMapData_);
  receivedMapData_.clear();

  // Wire controller
  controller_->setInputCallbacks(
      [this](uint32_t frame, uint8_t input) {
        transport_.sendInput(frame, input);
      },
      nullptr);
  controller_->setChecksumCallback(
      [this](uint32_t frame, uint32_t checksum) {
        transport_.sendChecksum(frame, checksum);
        onLocalChecksum(frame, checksum);
      });
  controller_->setPauseCallbacks(
      [this]() { transport_.sendPause(); },
      [this]() { transport_.sendResume(); });
  controller_->setEndMatchCallback(
      [this]() { transport_.sendEndMatch(); });
  for (uint32_t i = 0; i < 3; ++i)
    controller_->injectRemoteInput(i, 0);

  controllerPtr_ = controller_.get();

  // Flush any inputs that arrived before the controller was ready
  for (auto& pi : pendingInputs_) {
    controllerPtr_->injectRemoteInput(pi.frame, pi.input);
  }
  pendingInputs_.clear();

  localReady_ = false;
  remoteReady_ = false;
  handshakeReceived_ = false;
  sessionState_ = Playing;
}

void NetSession::generateAndSendMap() {
  // Generate the level on the host
  controller_->game.level.generateFromSettings(
      *controller_->game.common, *controller_->game.settings,
      controller_->game.rand);

  Level& level = controller_->game.level;

  // Serialize: width(2) + height(2) + rand_state_len(4) + rand_state(N)
  //          + rand_last(4) + pixel_data(w*h) + palette(768)
  uint16_t w = static_cast<uint16_t>(level.width);
  uint16_t h = static_cast<uint16_t>(level.height);
  std::string randState = controller_->game.rand.serialize();
  uint32_t randStateLen = static_cast<uint32_t>(randState.size());
  uint32_t randLast = controller_->game.rand.last;
  size_t pixelDataSize = static_cast<size_t>(w) * h;
  size_t rawSize = 4 + 4 + randStateLen + 4 + pixelDataSize + 768;

  std::vector<uint8_t> raw(rawSize);
  std::memcpy(raw.data(), &w, 2);
  std::memcpy(raw.data() + 2, &h, 2);
  std::memcpy(raw.data() + 4, &randStateLen, 4);
  std::memcpy(raw.data() + 8, randState.data(), randStateLen);
  std::memcpy(raw.data() + 8 + randStateLen, &randLast, 4);
  size_t pixelsOffset = 8 + randStateLen + 4;
  std::memcpy(raw.data() + pixelsOffset, level.data.data(), pixelDataSize);

  // Palette
  uint8_t* palPtr = raw.data() + pixelsOffset + pixelDataSize;
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
  mix(settings_->regenerateLevel ? 1u : 0u);
  mix(settings_->shadow ? 1u : 0u);
  mix(settings_->namesOnBonuses ? 1u : 0u);
  mix(static_cast<uint32_t>(settings_->bloodParticleMax));
  mix(static_cast<uint32_t>(settings_->zoneTimeout));

  return hash;
}

void NetSession::onChecksum(uint32_t frame, uint32_t remoteChecksum) {
  if (desyncDetected_ || sessionState_ != Playing || !controllerPtr_)
    return;

  // Look up our stored local checksum for this exact frame
  size_t slot = frame % CHECKSUM_BUFFER_SIZE;
  if (checksumBuffer_[slot].valid && checksumBuffer_[slot].frame == frame) {
    if (checksumBuffer_[slot].checksum != remoteChecksum) {
      desyncDetected_ = true;
      desyncFrame_ = frame;
      fprintf(stderr, "DESYNC DETECTED at frame %u! local=%08x remote=%08x\n",
              frame, checksumBuffer_[slot].checksum, remoteChecksum);
    }
  } else {
    // We haven't processed this frame yet — store for later comparison
    if (pendingRemoteCount_ < CHECKSUM_BUFFER_SIZE) {
      pendingRemoteChecksums_[pendingRemoteCount_++] = {frame, remoteChecksum};
    }
  }
}

void NetSession::onLocalChecksum(uint32_t frame, uint32_t checksum) {
  // Store in ring buffer
  size_t slot = frame % CHECKSUM_BUFFER_SIZE;
  checksumBuffer_[slot] = {frame, checksum, true};

  // Check pending remote checksums
  for (size_t i = 0; i < pendingRemoteCount_;) {
    if (pendingRemoteChecksums_[i].frame == frame) {
      if (pendingRemoteChecksums_[i].checksum != checksum) {
        desyncDetected_ = true;
        desyncFrame_ = frame;
        fprintf(stderr, "DESYNC DETECTED at frame %u! local=%08x remote=%08x\n",
                frame, checksum, pendingRemoteChecksums_[i].checksum);
      }
      // Remove by swapping with last
      pendingRemoteChecksums_[i] = pendingRemoteChecksums_[--pendingRemoteCount_];
    } else {
      ++i;
    }
  }
}
