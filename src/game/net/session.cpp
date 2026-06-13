#include "session.hpp"

#include <miniz.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "memoryFs.hpp"
#include "tcArchive.hpp"

NetSession::NetSession(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings,
                       FsNode tc_root)
    : common_(std::move(common)),
      settings_(std::move(settings)),

      localSettingsHash_(ComputeSettingsHash()),
      tcRoot_(std::move(tc_root)),

      localTcHash_(tc_archive::ComputeHash(tcRoot_)),
      originalTcName_(settings_->tc),
      originalCommon_(common_) {
  std::memset(&remotePlayerInfo_, 0, sizeof(remotePlayerInfo_));

  WireCallbacks();
}

NetSession::~NetSession() { Disconnect(); }

void NetSession::CreateController(int local_idx) {
  // Wire protocol caps inputDelay at kMaxRollback (see setInputDelay).
  // Clamp the settings value too so pre-fill loops below stay in sync
  // with the controller.
  if (settings_->input_delay < 0) {
    settings_->input_delay = 0;
  } else if (settings_->input_delay > rollback::kMaxRollback) {
    settings_->input_delay = rollback::kMaxRollback;
  }
  rollback_ = std::make_unique<RollbackController>(common_, settings_, local_idx);
  rollback_->SetInputDelay(static_cast<uint32_t>(settings_->input_delay));
}

void NetSession::WireActiveController() {
  auto checksum_cb = [this](uint8_t generation, uint32_t frame, uint32_t checksum) {
    transport_.SendChecksum(generation, frame, checksum);
    OnLocalChecksum(frame, checksum);
  };
  auto pause_cb = [this]() { transport_.SendPause(); };
  auto resume_cb = [this]() { transport_.SendResume(); };
  auto end_match_cb = [this]() { transport_.SendEndMatch(); };
  auto peer_left_cb = [this]() { transport_.SendPeerLeft(); };

  rollback_->SetInputCallbacks([this](uint8_t generation, uint32_t base_frame, uint8_t count,
                                      uint8_t const* inputs, uint32_t local_frame) {
    // localDelta = simFrame - baseFrame at send time. Encoded as
    // uint8_t; range guaranteed by the controller's K-wide window.
    auto const kLocalDelta = static_cast<uint8_t>(local_frame - base_frame);
    transport_.SendInputBatch(generation, base_frame, count, kLocalDelta, inputs);
  });
  rollback_->SetChecksumCallback(checksum_cb);
  rollback_->SetPauseCallbacks(pause_cb, resume_cb);
  rollback_->SetEndMatchCallback(end_match_cb);
  rollback_->SetPeerLeftCallback(peer_left_cb);
}

Game& NetSession::ActiveGame() { return rollback_->game; }

bool NetSession::HostGame(uint16_t port) {
  if (sessionState_ != kIdle) {
    return false;
  }

  role_ = kHost;
  gameSeed_ = static_cast<uint32_t>(std::time(nullptr));

  // Controller is created in tryStartGame once both sides have agreed
  // on settings. Pre-Playing transport callbacks tolerate a null
  // controller — buffered inputs flush on creation.

  if (!transport_.Host(port)) {
    sessionState_ = kFailed;
    return false;
  }
  sessionState_ = kWaitingForPeer;
  return true;
}

bool NetSession::JoinGame(const std::string& address, uint16_t port) {
  if (sessionState_ != kIdle) {
    return false;
  }

  role_ = kClient;
  gameSeed_ = 0;  // Will be set by host's handshake

  if (!transport_.Connect(address, port)) {
    sessionState_ = kFailed;
    return false;
  }
  sessionState_ = kWaitingForPeer;
  return true;
}

bool NetSession::HostWithTransport(NetTransport&& transport) {
  if (sessionState_ != kIdle) {
    return false;
  }

  role_ = kHost;
  gameSeed_ = static_cast<uint32_t>(std::time(nullptr));

  transport_ = std::move(transport);
  WireCallbacks();
  sessionState_ = kWaitingForPeer;
  return true;
}

bool NetSession::ConnectWithTransport(NetTransport&& transport, const std::string& peer_addr,
                                      uint16_t peer_port) {
  if (sessionState_ != kIdle) {
    return false;
  }

  role_ = kClient;
  gameSeed_ = 0;

  transport_ = std::move(transport);
  WireCallbacks();

  if (!transport_.ConnectExisting(peer_addr, peer_port)) {
    sessionState_ = kFailed;
    return false;
  }

  sessionState_ = kWaitingForPeer;
  return true;
}

void NetSession::Update() {
  if (sessionState_ == kIdle || sessionState_ == kFailed || sessionState_ == kDisconnected) {
    return;
  }

  transport_.Poll();

  if (transport_.CurrentState() == NetTransport::kFailed) {
    sessionState_ = kFailed;
    return;
  }

  if (transport_.CurrentState() == NetTransport::kDisconnected && sessionState_ != kIdle) {
    sessionState_ = kDisconnected;
    return;
  }
}

void NetSession::Disconnect() {
  transport_.Disconnect();
  // transport_.disconnect() tears down the ENet host but leaves the
  // std::function callbacks bound to `this`. They stay valid until the
  // next session replaces transport_ via host/connectWithTransport,
  // which calls wireCallbacks() itself.
  sessionState_ = kIdle;
  rollback_.reset();
  rollbackPtr_ = nullptr;
  handshakeReceived_ = false;
  handshakeSent_ = false;
  playerInfoReceived_ = false;
  matchSettingsReceived_ = false;
  mapDataReceived_ = false;
  localReady_ = false;
  remoteReady_ = false;
  receivedMapData_.clear();
  prePlayingInputBatches_.clear();

  // Restore client's original TC if it was changed during the session
  if (role_ == kClient && tcMemFs_) {
    settings_->tc = originalTcName_;
    if (on_tc_reloaded) {
      on_tc_reloaded(originalCommon_);
    }
    common_ = originalCommon_;
    tcMemFs_.reset();
  }
}

void NetSession::OnConnected() {
  sessionState_ = kHandshaking;

  // Host sends TC info first so client can verify/request TC data
  if (role_ == kHost) {
    transport_.SendTcInfo(localTcHash_, settings_->tc);
    tcResolved_ = true;  // Host always has correct TC
  }

  // Both sides send their handshake.
  // Host includes the seed; client sends 0 (host's seed is authoritative).
  uint32_t const kSeedToSend = (role_ == kHost) ? gameSeed_ : 0;
  transport_.SendHandshake(kSeedToSend, localSettingsHash_);
  handshakeSent_ = true;

  SendLocalPlayerInfo();

  // Host sends authoritative match settings
  if (role_ == kHost) {
    NetTransport::MatchSettingsData msd{};
    msd.lives = settings_->lives;
    msd.loading_time = settings_->loading_time;
    msd.game_mode = settings_->game_mode;
    msd.blood = settings_->blood;
    msd.max_bonuses = settings_->max_bonuses;
    msd.time_to_lose = settings_->time_to_lose;
    msd.flags_to_win = settings_->flags_to_win;
    msd.load_change = settings_->load_change ? 1 : 0;
    for (int i = 0; i < 40; ++i) {
      msd.weap_table[i] = settings_->weap_table[i];
    }
    msd.regenerate_level = settings_->regenerate_level ? 1 : 0;
    msd.shadow = settings_->shadow ? 1 : 0;
    msd.names_on_bonuses = settings_->names_on_bonuses ? 1 : 0;
    msd.blood_particle_max = settings_->blood_particle_max;
    msd.zone_timeout = settings_->zone_timeout;
    msd.input_delay = settings_->input_delay;
    transport_.SendMatchSettings(msd);
    matchSettingsReceived_ = true;  // Host already has correct settings
    mapDataReceived_ = true;        // Host generates locally
  }

  // Check if we can start (all data received)
  if (handshakeReceived_ && playerInfoReceived_ && matchSettingsReceived_ && mapDataReceived_ &&
      tcResolved_) {
    TryStartGame();
  }
}

void NetSession::OnDisconnected() { sessionState_ = kDisconnected; }

void NetSession::OnHandshake(uint32_t seed, uint32_t /*settings_hash*/) {
  // Client uses the host's seed
  if (role_ == kClient) {
    gameSeed_ = seed;
  }

  if (sessionState_ == kRematch) {
    // During rematch, handshake signals the host is starting the game.
    // Client needs to wait for map data before creating the controller.
    handshakeReceived_ = true;
    return;
  }

  handshakeReceived_ = true;

  if (handshakeSent_ && playerInfoReceived_ && matchSettingsReceived_ && mapDataReceived_ &&
      tcResolved_) {
    TryStartGame();
  }
}

void NetSession::OnRemoteInputBatch(uint8_t generation, uint32_t base_frame, uint8_t count,
                                    uint8_t const* inputs, uint32_t remote_local_frame) {
  if (rollbackPtr_) {
    rollbackPtr_->InjectRemoteBatch(generation, base_frame, count, inputs, remote_local_frame);
    return;
  }

  if (prePlayingInputBatches_.size() >= kMaxPrePlayingBatches) {
    return;
  }
  if (count > rollback::kMaxRollback + 1) {
    return;
  }
  PendingInputBatch b{};
  b.generation = generation;
  b.base_frame = base_frame;
  b.count = count;
  for (uint8_t i = 0; i < count; ++i) {
    b.inputs[i] = inputs[i];
  }
  b.remote_local_frame = remote_local_frame;
  prePlayingInputBatches_.push_back(b);
}

void NetSession::OnPlayerInfo(const NetTransport::PlayerInfo& info) {
  remotePlayerInfo_ = info;
  playerInfoReceived_ = true;

  // During Rematch, PlayerInfo is re-sent so startRematch{,Client}() can
  // pick up the peer's latest weapons. The rematch start is driven by
  // toggleReady / onHandshake, not by this packet.
  if (sessionState_ == kRematch) {
    return;
  }

  if (handshakeSent_ && handshakeReceived_ && matchSettingsReceived_ && mapDataReceived_ &&
      tcResolved_) {
    TryStartGame();
  }
}

void NetSession::OnMatchSettings(const NetTransport::MatchSettingsData& data) {
  // Client applies host's authoritative settings
  if (role_ == kClient) {
    settings_->lives = data.lives;
    settings_->loading_time = data.loading_time;
    settings_->game_mode = data.game_mode;
    settings_->blood = data.blood;
    settings_->max_bonuses = data.max_bonuses;
    settings_->time_to_lose = data.time_to_lose;
    settings_->flags_to_win = data.flags_to_win;
    settings_->load_change = data.load_change != 0;
    for (int i = 0; i < 40; ++i) {
      settings_->weap_table[i] = data.weap_table[i];
    }
    settings_->regenerate_level = data.regenerate_level != 0;
    settings_->shadow = data.shadow != 0;
    settings_->names_on_bonuses = data.names_on_bonuses != 0;
    settings_->blood_particle_max = data.blood_particle_max;
    settings_->zone_timeout = data.zone_timeout;
    // inputDelay is host-authoritative.
    settings_->input_delay = data.input_delay;
  }

  matchSettingsReceived_ = true;

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ && mapDataReceived_ &&
      tcResolved_) {
    TryStartGame();
  }
}

void NetSession::OnMapData(const void* data, size_t len) {
  if (role_ != kClient) {
    return;
  }

  // Reject oversized map data to prevent memory exhaustion.
  // 256 MB covers the worst case: 4096×4096 with full MODERNLV layers
  // (~112 MB uncompressed), with headroom for future growth.
  static constexpr size_t kMaxMapData = 256 * 1024 * 1024;
  if (len > kMaxMapData) {
    std::fprintf(stderr, "OnMapData: rejected oversized packet (%zu bytes > %zu limit)\n", len,
                 kMaxMapData);
    return;
  }

  receivedMapData_.assign(static_cast<const uint8_t*>(data),
                          static_cast<const uint8_t*>(data) + len);
  mapDataReceived_ = true;

  if (sessionState_ == kRematch && handshakeReceived_) {
    // We have the seed and map — start the client-side rematch
    StartRematchClient();
    return;
  }

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ && matchSettingsReceived_ &&
      tcResolved_) {
    TryStartGame();
  }
}

void NetSession::OnPause() {
  if (rollbackPtr_) {
    rollbackPtr_->SetRemotePaused(/*paused=*/true);
  }
}

void NetSession::OnResume() {
  if (rollbackPtr_) {
    rollbackPtr_->SetRemotePaused(/*paused=*/false);
  }
}

void NetSession::OnRemoteEndMatch() {
  if (rollbackPtr_) {
    rollbackPtr_->EndMatch();
  }
}

void NetSession::OnRemotePeerLeft() {
  if (rollbackPtr_) {
    rollbackPtr_->PeerLeft();
  }
}

void NetSession::SendPause() { transport_.SendPause(); }

void NetSession::SendResume() { transport_.SendResume(); }

void NetSession::WireCallbacks() {
  transport_.on_connected = [this]() { OnConnected(); };
  transport_.on_disconnected = [this]() { OnDisconnected(); };
  transport_.on_handshake = [this](uint32_t seed, uint32_t hash) { OnHandshake(seed, hash); };
  transport_.on_remote_input_batch = [this](uint8_t generation, uint32_t base_frame, uint8_t count,
                                            uint8_t const* inputs, uint32_t remote_local_frame) {
    OnRemoteInputBatch(generation, base_frame, count, inputs, remote_local_frame);
  };
  transport_.on_player_info = [this](const NetTransport::PlayerInfo& info) { OnPlayerInfo(info); };
  transport_.on_match_settings = [this](const NetTransport::MatchSettingsData& data) {
    OnMatchSettings(data);
  };
  transport_.on_map_data = [this](const void* data, size_t len) { OnMapData(data, len); };
  transport_.on_pause = [this]() { OnPause(); };
  transport_.on_resume = [this]() { OnResume(); };
  transport_.on_end_match = [this]() { OnRemoteEndMatch(); };
  transport_.on_peer_left = [this]() { OnRemotePeerLeft(); };
  transport_.on_checksum = [this](uint8_t generation, uint32_t frame, uint32_t checksum) {
    OnChecksum(generation, frame, checksum);
  };
  transport_.on_rematch_ready = [this](bool ready) { OnRematchReady(ready); };
  transport_.on_rematch_level = [this](bool random, std::string file) {
    OnRematchLevel(random, std::move(file));
  };
  transport_.on_tc_info = [this](uint32_t hash, const std::string& name) { OnTcInfo(hash, name); };
  transport_.on_tc_response = [this](bool need_data) { OnTcResponse(need_data); };
  transport_.on_tc_data = [this](const void* data, size_t len) { OnTcData(data, len); };
}

void NetSession::OnTcInfo(uint32_t hash, const std::string& name) {
  // Client receives TC info from host
  if (role_ != kClient) {
    return;
  }

  if (name == settings_->tc && hash == localTcHash_) {
    // Same TC, no transfer needed
    transport_.SendTcResponse(/*need_data=*/false);
    tcResolved_ = true;

    if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ && matchSettingsReceived_ &&
        mapDataReceived_) {
      TryStartGame();
    }
  } else {
    // Different TC — request the data
    settings_->tc = name;
    transport_.SendTcResponse(/*need_data=*/true);
  }
}

void NetSession::OnTcResponse(bool need_data) {
  // Host receives client's response about TC
  if (role_ != kHost) {
    return;
  }

  if (need_data) {
    // Client needs the TC archive — pack and send it
    auto archive = tc_archive::Pack(tcRoot_);
    transport_.SendTcData(archive.data(), archive.size());
  }
  // If !needData, the client already has the TC — nothing more to do
}

void NetSession::OnTcData(const void* data, size_t len) {
  // Client receives TC archive from host
  if (role_ != kClient) {
    return;
  }

  // Reject oversized TC data
  static constexpr size_t kMaxTcData = 50 * 1024 * 1024;  // 50 MB
  if (len > kMaxTcData) {
    return;
  }

  auto files = tc_archive::Unpack(static_cast<const uint8_t*>(data), len);
  if (files.empty()) {
    return;
  }

  // Load TC from memory (no disk writes — platform-agnostic)
  auto mem_fs = std::make_shared<MemoryFs>();
  for (auto& file : files) {
    mem_fs->files[file.name] = std::move(file.data);
  }

  // Keep the MemoryFs alive by storing it in the session
  tcMemFs_ = mem_fs;

  // Reload Common from the in-memory TC
  auto new_common = std::make_shared<Common>();
  new_common->load(mem_fs->Root());

  common_ = new_common;

  if (on_tc_reloaded) {
    on_tc_reloaded(new_common);
  }

  tcResolved_ = true;

  if (handshakeSent_ && handshakeReceived_ && playerInfoReceived_ && matchSettingsReceived_ &&
      mapDataReceived_) {
    TryStartGame();
  }
}

void NetSession::ApplyRemotePlayerInfo(int remote_idx) {
  Worm* remote_worm = ActiveGame().WormByIdx(remote_idx);
  // Create a distinct copy so we don't mutate the saved player profile
  auto remote_ws = std::make_shared<WormSettings>(*remote_worm->settings);
  for (int i = 0; i < 5; ++i) {
    remote_ws->weapons[i] = remotePlayerInfo_.weapons[i];
  }
  remote_ws->color = remotePlayerInfo_.color;
  for (int i = 0; i < 3; ++i) {
    remote_ws->rgb[i] = remotePlayerInfo_.rgb[i];
  }
  remote_ws->name.assign(remotePlayerInfo_.name,
                         strnlen(remotePlayerInfo_.name, sizeof(remotePlayerInfo_.name)));
  remote_worm->settings = remote_ws;
}

void NetSession::PrefillRemoteInput() {
  // Pre-fill exactly inputDelay frames of empty remote input. These are
  // the frames whose remote input the remote peer literally cannot have
  // recorded yet (with inputDelay=D, the remote first records its
  // localInputs[D] at simFrame=0). Pre-filling further would overwrite
  // frames the remote *will* fill in — injectRemoteInput drops frames
  // <= confirmedSimFrame_, so the rollback path couldn't correct later.
  auto const kPreFillCount = static_cast<uint32_t>(settings_->input_delay);
  for (uint32_t i = 0; i < kPreFillCount; ++i) {
    rollback_->InjectRemoteInput(i, 0);
  }
}

void NetSession::BeginPlaying(int local_idx, bool is_rematch) {
  int const kRemoteIdx = 1 - local_idx;
  CreateController(local_idx);
  ApplyRemotePlayerInfo(kRemoteIdx);
  ActiveGame().rand.Seed(gameSeed_);

  if (role_ == kHost) {
    GenerateAndSendMap();
    rollback_->SetLevelPreloaded();
  } else {
    rollback_->LoadLevelFromData(receivedMapData_);
    receivedMapData_.clear();
  }

  WireActiveController();
  PrefillRemoteInput();
  rollbackPtr_ = rollback_.get();

  for (auto const& b : prePlayingInputBatches_) {
    rollbackPtr_->InjectRemoteBatch(b.generation, b.base_frame, b.count, b.inputs.data(),
                                    b.remote_local_frame);
  }
  prePlayingInputBatches_.clear();

  if (is_rematch) {
    localReady_ = false;
    remoteReady_ = false;
    // Client clears handshakeReceived_ so the next rematch's seed can land.
    if (role_ == kClient) {
      handshakeReceived_ = false;
    }
  }

  sessionState_ = kPlaying;
}

void NetSession::TryStartGame() {
  if (sessionState_ == kPlaying) {
    return;
  }
  BeginPlaying((role_ == kHost) ? 0 : 1, /*is_rematch=*/false);
}

void NetSession::EnterRematch() {
  if (sessionState_ != kPlaying) {
    return;
  }

  sessionState_ = kRematch;
  localReady_ = false;
  remoteReady_ = false;

  // The old controller is still owned by Gfx. Clear our raw pointer since
  // it will be replaced when the rematch starts.
  rollbackPtr_ = nullptr;

  // Reset per-game handshake flags for the next round
  mapDataReceived_ = (role_ == kHost);  // host generates locally
  receivedMapData_.clear();

  // Refresh the peer's view of our weapons. Without this, startRematch
  // {,Client}() would reapply the connect-time profile and each peer
  // would see the other's pre-match weapons in round 2's weapon select.
  SendLocalPlayerInfo();
}

void NetSession::SendLocalPlayerInfo() {
  auto& net_ws = settings_->worm_settings[Settings::kNetworkPlayerIdx];
  NetTransport::PlayerInfo info{};
  for (int i = 0; i < 5; ++i) {
    info.weapons[i] = net_ws->weapons[i];
  }
  info.color = net_ws->color;
  for (int i = 0; i < 3; ++i) {
    info.rgb[i] = net_ws->rgb[i];
  }
  std::strncpy(info.name, net_ws->name.c_str(), sizeof(info.name) - 1);
  info.name[sizeof(info.name) - 1] = '\0';
  transport_.SendPlayerInfo(info);
}

void NetSession::ToggleReady() {
  if (sessionState_ != kRematch) {
    return;
  }

  localReady_ = !localReady_;
  transport_.SendRematchReady(localReady_);

  // Only host initiates the rematch start
  if (role_ == kHost && localReady_ && remoteReady_) {
    StartRematch();
  }
}

void NetSession::SetRematchLevel(bool random_level, const std::string& level_file) {
  if (sessionState_ != kRematch || role_ != kHost) {
    return;
  }

  settings_->random_level = random_level;
  settings_->level_file = level_file;

  transport_.SendRematchLevel(random_level, level_file);

  // Reset ready states when level changes
  if (localReady_) {
    localReady_ = false;
    transport_.SendRematchReady(/*ready=*/false);
  }
  if (remoteReady_) {
    remoteReady_ = false;
  }
}

void NetSession::OnRematchReady(bool ready) {
  if (sessionState_ != kRematch) {
    return;
  }

  remoteReady_ = ready;

  // Only host initiates the rematch start
  if (role_ == kHost && localReady_ && remoteReady_) {
    StartRematch();
  }
}

void NetSession::OnRematchLevel(bool random_level, std::string level_file) {
  if (sessionState_ != kRematch || role_ != kClient) {
    return;
  }

  settings_->random_level = random_level;
  settings_->level_file = std::move(level_file);

  // Reset ready states when level changes
  localReady_ = false;
  remoteReady_ = false;
}

void NetSession::StartRematch() {
  // Only the host calls this directly. The client starts via onHandshake+onMapData.
  if (sessionState_ != kRematch || role_ != kHost) {
    return;
  }

  // Generate a new seed and send it before beginPlaying generates the map
  gameSeed_ = static_cast<uint32_t>(std::time(nullptr));
  transport_.SendHandshake(gameSeed_, 0);

  BeginPlaying(/*local_idx=*/0, /*is_rematch=*/true);
}

void NetSession::StartRematchClient() {
  // Called on the client side when handshake (seed) + map data are both received
  if (sessionState_ != kRematch || role_ != kClient) {
    return;
  }

  BeginPlaying(/*local_idx=*/1, /*is_rematch=*/true);
}

void NetSession::GenerateAndSendMap() {
  // Generate the level on the host
  Game& g = ActiveGame();
  g.level.GenerateFromSettings(*g.common, *g.settings, g.rand);

  Level& level = g.level;

  // Serialize: width(2) + height(2) + rand_state_len(4) + rand_state(N)
  //          + rand_last(4) + pixel_data(w*h) + palette(768)
  auto w = static_cast<uint16_t>(level.width);
  auto h = static_cast<uint16_t>(level.height);
  std::string rand_state = g.rand.serialize();
  auto rand_state_len = static_cast<uint32_t>(rand_state.size());
  uint32_t rand_last = g.rand.last;
  size_t const kPixelDataSize = static_cast<size_t>(w) * h;
  bool const kHasDisplay = !level.display_data.empty();
  bool const kHasAnim = kHasDisplay && !level.argb_ramps.empty();
  // has_display_layer(1) + display_data(kPixelDataSize*4) + display_valid(kPixelDataSize)
  size_t const kDisplayExtra = kHasDisplay ? 1 + kPixelDataSize * 4 + kPixelDataSize : 1;
  // anim section: ramp_count(1) + per-ramp[shift(1)+color_count(2)+colors(N*4)] + display_anim
  size_t anim_extra = 1;  // ramp_count byte (0 if no anim)
  if (kHasAnim) {
    for (auto const& ramp : level.argb_ramps) {
      anim_extra += 1 + 2 + ramp.colors.size() * 4;
    }
    anim_extra += kPixelDataSize;
  }
  size_t const kRawSize =
      4 + 4 + rand_state_len + 4 + kPixelDataSize + 768 + kDisplayExtra + anim_extra;

  std::vector<uint8_t> raw(kRawSize);
  std::memcpy(raw.data(), &w, 2);
  std::memcpy(raw.data() + 2, &h, 2);
  std::memcpy(raw.data() + 4, &rand_state_len, 4);
  std::memcpy(raw.data() + 8, rand_state.data(), rand_state_len);
  std::memcpy(raw.data() + 8 + rand_state_len, &rand_last, 4);
  size_t const kPixelsOffset = 8 + rand_state_len + 4;
  std::memcpy(raw.data() + kPixelsOffset, level.material_id.data(), kPixelDataSize);

  // Palette
  uint8_t* pal_ptr = raw.data() + kPixelsOffset + kPixelDataSize;
  for (int i = 0; i < 256; ++i) {
    pal_ptr[i * 3 + 0] = level.origpal.entries[i].r;
    pal_ptr[i * 3 + 1] = level.origpal.entries[i].g;
    pal_ptr[i * 3 + 2] = level.origpal.entries[i].b;
  }

  // Display layer: has_display_layer(1) + [display_data(kPixelDataSize*4) +
  // display_valid(kPixelDataSize)] when present.
  uint8_t* disp_ptr = pal_ptr + 768;
  disp_ptr[0] = kHasDisplay ? 1 : 0;
  if (kHasDisplay) {
    std::memcpy(disp_ptr + 1, level.display_data.data(), kPixelDataSize * sizeof(uint32_t));
    std::memcpy(disp_ptr + 1 + kPixelDataSize * 4, level.display_valid.data(), kPixelDataSize);
  }

  // Anim layer: ramp_count(1) + [shift(1)+color_count(2LE)+colors(N*4)]... + display_anim(cells)
  uint8_t* anim_ptr = disp_ptr + (kHasDisplay ? 1 + kPixelDataSize * 4 + kPixelDataSize : 1);
  if (kHasAnim) {
    auto const kRampCount = static_cast<uint8_t>(level.argb_ramps.size());
    *anim_ptr++ = kRampCount;
    for (auto const& ramp : level.argb_ramps) {
      *anim_ptr++ = ramp.shift;
      auto const kColorCount = static_cast<uint16_t>(ramp.colors.size());
      std::memcpy(anim_ptr, &kColorCount, 2);
      anim_ptr += 2;
      std::memcpy(anim_ptr, ramp.colors.data(), ramp.colors.size() * 4);
      anim_ptr += ramp.colors.size() * 4;
    }
    std::memcpy(anim_ptr, level.display_anim.data(), kPixelDataSize);
  } else {
    *anim_ptr = 0;  // ramp_count = 0
  }

  // Compress with miniz
  mz_ulong const kCompBound = mz_compressBound(static_cast<mz_ulong>(kRawSize));
  std::vector<uint8_t> compressed(kCompBound);
  mz_ulong comp_size = kCompBound;
  int const kStatus =
      mz_compress(compressed.data(), &comp_size, raw.data(), static_cast<mz_ulong>(kRawSize));
  if (kStatus == MZ_OK) {
    compressed.resize(comp_size);
  } else {
    // Fallback: send uncompressed
    compressed = std::move(raw);
  }

  // Send: compressed flag(1) + uncompressedSize(4) + data
  std::vector<uint8_t> packet(1 + 4 + compressed.size());
  packet[0] = (kStatus == MZ_OK) ? 1 : 0;
  auto raw_size32 = static_cast<uint32_t>(kRawSize);
  std::memcpy(packet.data() + 1, &raw_size32, 4);
  std::memcpy(packet.data() + 5, compressed.data(), compressed.size());

  transport_.SendMapData(packet.data(), packet.size());
}

std::unique_ptr<Controller> NetSession::ReleaseController() {
  // Return the controller as a polymorphic Controller. The session keeps
  // its typed raw pointer for inject/pause/end dispatch.
  return std::move(rollback_);
}

uint32_t NetSession::ComputeSettingsHash() const {
  // Hash the gameplay-relevant settings to detect mismatches.
  // Uses a simple FNV-1a hash over the key fields.
  uint32_t hash = 2166136261U;
  auto mix = [&hash](uint32_t val) {
    for (int i = 0; i < 4; ++i) {
      hash ^= (val >> (i * 8)) & 0xFF;
      hash *= 16777619U;
    }
  };

  mix(settings_->lives);
  mix(settings_->load_change ? 1U : 0U);
  mix(settings_->max_bonuses);
  mix(settings_->blood);
  mix(settings_->time_to_lose);
  mix(settings_->flags_to_win);
  mix(settings_->game_mode);
  mix(settings_->loading_time);
  mix(settings_->regenerate_level ? 1U : 0U);
  mix(settings_->shadow ? 1U : 0U);
  mix(settings_->names_on_bonuses ? 1U : 0U);
  mix(static_cast<uint32_t>(settings_->blood_particle_max));
  mix(static_cast<uint32_t>(settings_->zone_timeout));

  return hash;
}

namespace {
// Behind OPENLIERO_CHECKSUM_LOG=1 we periodically dump counts of
// checksums sent locally and received from the peer. If both stay 0
// the desync detector is silent because no data is flowing, not
// because state matches.
bool ChecksumLogEnabled() {
  static int v = -1;
  if (v < 0) {
    char const* e = std::getenv("OPENLIERO_CHECKSUM_LOG");
    v = (e && *e && *e != '0') ? 1 : 0;
  }
  return v != 0;
}

void MaybeLog(char const* who, uint64_t& counter, uint32_t frame, uint32_t checksum) {
  if (!ChecksumLogEnabled()) {
    return;
  }
  ++counter;
  if (counter % 70 == 0) {  // ~1 second at 70 Hz
    std::fprintf(stderr, "[checksum %s] count=%llu frame=%u value=%08x\n", who,
                 static_cast<unsigned long long>(counter), frame, checksum);
  }
}
}  // namespace

void NetSession::OnChecksum(uint8_t generation, uint32_t frame, uint32_t remote_checksum) {
  if (desyncDetected_ || sessionState_ != kPlaying || !rollbackPtr_) {
    return;
  }

  // Drop pre-transition checksums: they describe the peer's old
  // simFrame numbering and would compare against a WS-phase slot that
  // no longer exists in our ring.
  if (generation != rollbackPtr_->Generation()) {
    return;
  }

  static uint64_t remote_count = 0;
  MaybeLog("remote", remote_count, frame, remote_checksum);

  // Look up our stored local checksum for this exact frame
  size_t const kSlot = frame % kChecksumBufferSize;
  if (checksumBuffer_[kSlot].valid && checksumBuffer_[kSlot].frame == frame) {
    if (checksumBuffer_[kSlot].checksum != remote_checksum) {
      desyncDetected_ = true;
      desyncFrame_ = frame;
      std::fprintf(stderr, "DESYNC DETECTED at frame %u! local=%08x remote=%08x\n", frame,
                   checksumBuffer_[kSlot].checksum, remote_checksum);
    }
  } else {
    // We haven't processed this frame yet — store for later comparison
    if (pendingRemoteCount_ < kChecksumBufferSize) {
      pendingRemoteChecksums_[pendingRemoteCount_++] = {.frame = frame,
                                                        .checksum = remote_checksum};
    }
  }
}

void NetSession::OnLocalChecksum(uint32_t frame, uint32_t checksum) {
  static uint64_t local_count = 0;
  MaybeLog("local", local_count, frame, checksum);

  // Store in ring buffer
  size_t const kSlot = frame % kChecksumBufferSize;
  checksumBuffer_[kSlot] = {.frame = frame, .checksum = checksum, .valid = true};

  // Check pending remote checksums
  for (size_t i = 0; i < pendingRemoteCount_;) {
    if (pendingRemoteChecksums_[i].frame == frame) {
      if (pendingRemoteChecksums_[i].checksum != checksum) {
        desyncDetected_ = true;
        desyncFrame_ = frame;
        std::fprintf(stderr, "DESYNC DETECTED at frame %u! local=%08x remote=%08x\n", frame,
                     checksum, pendingRemoteChecksums_[i].checksum);
      }
      // Remove by swapping with last
      pendingRemoteChecksums_[i] = pendingRemoteChecksums_[--pendingRemoteCount_];
    } else {
      ++i;
    }
  }
}
