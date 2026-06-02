#include "rollbackController.hpp"

#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../replay.hpp"
#include "../spectatorviewport.hpp"
#include "../viewport.hpp"

#include <miniz.h>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Shared two-player setup for the live and shadow games. Both must
// produce identical processFrame paths, so the worm slots and main
// viewports are configured here from a single source of truth. The
// live ctor also adds a SpectatorViewport; the shadow does not need
// one.
static void ConfigureGameSlots(Game& g, std::array<std::shared_ptr<WormSettings>, 2> ws) {
  for (int idx = 0; idx < 2; ++idx) {
    auto worm = std::make_shared<Worm>();
    worm->settings = ws[idx];
    worm->health = worm->settings->health;
    worm->index = idx;
    worm->stats_x = idx == 0 ? 0 : 218;
    g.AddWorm(worm);
  }
  g.AddViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  g.AddViewport(new Viewport(Rect(160, 0, 158 + 160, 158), 1, 504, 350));
}

RollbackController::RollbackController(std::shared_ptr<Common> common,
                                       std::shared_ptr<Settings> settings, int local_player_idx)
    : game(common, settings, gfx.sound_player),
      localIdx_(local_player_idx),
      remoteIdx_(local_player_idx ^ 1),
      state_(kStateInitial),
      fadeValue_(0),
      goingToMenu_(false),
      simFrame_(0),
      inputDelay_(3),
      lastSentFrame_(0),
      lastSentFrameValid_(false),
      rollbackBufferPrepared_(false),
      confirmedSimFrame_(-1),
      lastRemoteInput_(0) {
  localPrevInput_ = 0;
  remotePrevInput_ = 0;
  localHeldFrames_.fill(0);
  remoteHeldFrames_.fill(0);
  skipWeaponSelection_ = false;
  levelPreloaded_ = false;
  localPaused_ = false;
  remotePaused_ = false;

  pauseMenu_.Init(true);
  pauseMenu_.AddItem(MenuItem(7, 6, "RESUME", 0));
  pauseMenu_.AddItem(MenuItem(7, 6, "END MATCH", 2));
  pauseMenu_.AddItem(MenuItem(7, 6, "DISCONNECT", 1));

  localInputs_.fill(0);
  remoteInputs_.fill(0);
  remoteInputReady_.fill(false);

  std::array<std::shared_ptr<WormSettings>, 2> ws;
  for (int idx = 0; idx < 2; ++idx) {
    ws[idx] = (idx == localIdx_) ? settings->worm_settings[Settings::kNetworkPlayerIdx]
                                 : settings->worm_settings[idx];
  }
  ConfigureGameSlots(game, ws);
  game.AddSpectatorViewport(new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
}

RollbackController::~RollbackController() {}

void RollbackController::LoadLevelFromData(const std::vector<uint8_t>& data) {
  if (data.size() < 5) return;

  bool is_compressed = (data[0] != 0);
  uint32_t raw_size;
  std::memcpy(&raw_size, data.data() + 1, 4);

  static constexpr uint32_t kMaxRawSize = 10 * 1024 * 1024;
  if (raw_size > kMaxRawSize) return;

  std::vector<uint8_t> raw;
  if (is_compressed) {
    raw.resize(raw_size);
    mz_ulong dest_len = raw_size;
    int status = mz_uncompress(raw.data(), &dest_len, data.data() + 5,
                               static_cast<mz_ulong>(data.size() - 5));
    if (status != MZ_OK) return;
  } else {
    raw.assign(data.begin() + 5, data.end());
  }

  if (raw.size() < 8) return;

  uint16_t w, h;
  std::memcpy(&w, raw.data(), 2);
  std::memcpy(&h, raw.data() + 2, 2);

  if (w == 0 || h == 0 || w > 4096 || h > 4096) return;

  uint32_t rand_state_len;
  std::memcpy(&rand_state_len, raw.data() + 4, 4);

  if (rand_state_len > 64 * 1024) return;
  if (raw.size() < 8 + rand_state_len + 4) return;

  std::string rand_state(reinterpret_cast<const char*>(raw.data() + 8), rand_state_len);
  uint32_t rand_last;
  std::memcpy(&rand_last, raw.data() + 8 + rand_state_len, 4);

  size_t pixels_offset = 8 + rand_state_len + 4;
  size_t pixel_data_size = static_cast<size_t>(w) * h;
  if (raw.size() < pixels_offset + pixel_data_size + 768) return;

  game.level.Resize(w, h);
  Common& common = *game.common;

  const uint8_t* pixels = raw.data() + pixels_offset;
  for (size_t i = 0; i < pixel_data_size; ++i) {
    game.level.data[i] = pixels[i];
    game.level.materials[i] = common.materials[pixels[i]];
  }

  const uint8_t* pal_data = raw.data() + pixels_offset + pixel_data_size;
  for (int i = 0; i < 256; ++i) {
    game.level.origpal.entries[i].r = pal_data[i * 3 + 0];
    game.level.origpal.entries[i].g = pal_data[i * 3 + 1];
    game.level.origpal.entries[i].b = pal_data[i * 3 + 2];
  }

  game.rand.Deserialize(rand_state);
  game.rand.last = rand_last;

  levelPreloaded_ = true;
}

void RollbackController::SetInputCallbacks(InputBatchSendCallback send) {
  sendInputBatch_ = std::move(send);
}

void RollbackController::SendInputWindow(uint32_t newest_frame, uint32_t local_frame) {
  if (!sendInputBatch_) return;
  constexpr uint8_t kK = static_cast<uint8_t>(rollback::kMaxRollback + 1);
  uint8_t count;
  uint32_t base_frame;
  if (newest_frame + 1u < kK) {
    count = static_cast<uint8_t>(newest_frame + 1u);
    base_frame = 0;
  } else {
    count = kK;
    base_frame = newest_frame - (kK - 1u);
  }
  std::array<uint8_t, kK> window{};
  for (uint8_t i = 0; i < count; ++i) {
    window[i] = localInputs_[(base_frame + i) % kInputBufferSize];
  }
  sendInputBatch_(generation_, base_frame, count, window.data(), local_frame);
}

void RollbackController::InjectRemoteBatch(uint8_t generation, uint32_t base_frame, uint8_t count,
                                           uint8_t const* inputs, uint32_t remote_local_frame) {
  // Same-generation packets feed the input ring; gen+1 packets are
  // buffered until our own phase transition fires (replayed in
  // resetForGamePhase). Older or further-future packets are unrecoverable.
  if (generation == generation_) {
    for (uint8_t i = 0; i < count; ++i) {
      InjectRemoteInput(base_frame + i, inputs[i]);
    }
    // Monotonic — an out-of-order stale packet must not pull our
    // knowledge of the remote's progress backwards.
    int32_t f = static_cast<int32_t>(remote_local_frame);
    if (f > lastKnownRemoteFrame_) lastKnownRemoteFrame_ = f;
    return;
  }

  if (generation == static_cast<uint8_t>(generation_ + 1) && count <= kMaxPendingFutureBatches) {
    if (pendingFutureCount_ < kMaxPendingFutureBatches) {
      auto& slot = pendingFutureBatches_[pendingFutureCount_++];
      slot.base_frame = base_frame;
      slot.count = count;
      for (uint8_t i = 0; i < count; ++i) slot.inputs[i] = inputs[i];
      slot.remote_local_frame = remote_local_frame;
      return;
    }
  }

  ++droppedOldGenerationBatches_;
}

void RollbackController::InjectRemoteInput(uint32_t frame, uint8_t input) {
  // Redundant batch packets routinely overlap the confirmation boundary;
  // re-injecting already-confirmed frames would re-set remoteInputReady
  // on a ring slot that wraps into the live rollback window.
  if (static_cast<int32_t>(frame) <= confirmedSimFrame_) return;
  uint32_t slot = frame % kInputBufferSize;
  remoteInputs_[slot] = input;
  remoteInputReady_[slot] = true;
}

void RollbackController::OnKey(int key, bool key_state) {
  Worm::Control control;
  Worm* worm = game.worms[localIdx_].get();
  bool found = false;

  if (worm->settings->input_device == WormSettingsExtensions::kInputKeyboard) {
    uint32_t* controls =
        game.settings->kExtensions ? worm->settings->controls_ex : worm->settings->controls;
    std::size_t max_control =
        game.settings->kExtensions ? WormSettings::kMaxControlEx : WormSettings::kMaxControl;
    for (std::size_t c = 0; c < max_control; ++c) {
      if (controls[c] == static_cast<uint32_t>(key)) {
        control = static_cast<Worm::Control>(c);
        found = true;
        break;
      }
    }
  }

  if (found) {
    worm->clean_control_states.Set(control, key_state);

    if (control < Worm::kMaxControl) {
      localControlState_.Set(control, key_state);
    }

    if (worm->clean_control_states[WormSettings::kDig]) {
      localControlState_.Set(Worm::kLeft, true);
      localControlState_.Set(Worm::kRight, true);
    } else {
      if (!worm->clean_control_states[Worm::kLeft]) localControlState_.Set(Worm::kLeft, false);
      if (!worm->clean_control_states[Worm::kRight]) localControlState_.Set(Worm::kRight, false);
    }
  }

  if (key == kDkEscape && key_state) {
    if (localPaused_) {
      localPaused_ = false;
      if (onLocalResume_) onLocalResume_();
    } else if (remotePaused_ && !goingToMenu_) {
      // Remote is paused, local Escapes — treat as a disconnect so the
      // peer learns and tears down in lockstep instead of waiting for
      // socket timeout.
      if (onPeerLeft_) onPeerLeft_();
      PeerLeft();
    } else if (!goingToMenu_) {
      localPaused_ = true;
      pauseMenu_.MoveToFirstVisible();
      if (onLocalPause_) onLocalPause_();
    }
  }
}

void RollbackController::Unfocus() {
  if (state_ == kStateWeaponSelection && ws_) ws_->Unfocus();
  localPaused_ = false;
  remotePaused_ = false;
}

void RollbackController::Focus() {
  if (state_ == kStateGameEnded) {
    goingToMenu_ = true;
    fadeValue_ = 0;
    return;
  }
  if (state_ == kStateWeaponSelection) ws_->Focus();
  if (state_ == kStateInitial) {
    if (!levelPreloaded_) game.level.GenerateFromSettings(*game.common, *game.settings, game.rand);

    if (skipWeaponSelection_) {
      for (auto const& w : game.worms) w->InitWeapons(game);
      for (auto const& w : game.worms) w->lives = game.settings->lives;
      game.StartGame();
      game.ResetWorms();
      state_ = kStateGame;

      SeedRollbackAndShadow();
    } else {
      state_ = kStateWeaponSelection;

      for (auto const& w : game.worms) {
        w->settings->controller = 0;
      }

      ws_ = std::make_unique<WeaponSelection>(game);
    }
  }
  game.Focus(gfx.play_renderer);
  game.Focus(gfx.single_screen_renderer);
  goingToMenu_ = false;
  fadeValue_ = 0;

  // Size the rollback ring buffer once the level (and therefore the
  // GameSnapshot vector sizes) are known.
  if (!rollbackBufferPrepared_) {
    rollbackBuffer_.Prepare(game);
    rollbackBufferPrepared_ = true;
  }
}

bool RollbackController::Process() {
  if (IsPaused()) {
    if (fadeValue_ < 33) fadeValue_ += 1;

    if (localPaused_) {
      if (gfx.TestSdlKeyOnce(SDL_SCANCODE_UP) || gfx.TestControlOnce(WormSettingsExtensions::kUp) ||
          gfx.TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        g_sound_player->Play(game.common->sound_hook[SoundMenuMoveDown]);
        pauseMenu_.Movement(-1);
      }

      if (gfx.TestSdlKeyOnce(SDL_SCANCODE_DOWN) ||
          gfx.TestControlOnce(WormSettingsExtensions::kDown) ||
          gfx.TestGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        g_sound_player->Play(game.common->sound_hook[SoundMenuMoveUp]);
        pauseMenu_.Movement(1);
      }

      if (gfx.TestSdlKeyOnce(SDL_SCANCODE_RETURN) || gfx.TestSdlKeyOnce(SDL_SCANCODE_KP_ENTER) ||
          gfx.TestControlOnce(WormSettingsExtensions::kFire) ||
          gfx.TestGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
        int sel = pauseMenu_.SelectedId();
        if (sel == 0) {
          localPaused_ = false;
          if (onLocalResume_) onLocalResume_();
        } else if (sel == 2) {
          localPaused_ = false;
          if (onLocalResume_) onLocalResume_();
          if (onEndMatch_) onEndMatch_();
          EndMatch();
        } else {
          if (onLocalResume_) onLocalResume_();
          if (onPeerLeft_) onPeerLeft_();
          PeerLeft();
        }
      }
    }

    return true;
  }

  lastTickResimFrames_ = 0;

  if (state_ == kStateWeaponSelection) {
    AdvanceWeaponSelection();
  } else if (state_ == kStateGame || state_ == kStateGameEnded) {
    AdvanceSimulation();
  }

  if (goingToMenu_) {
    if (fadeValue_ > 0)
      fadeValue_ -= 1;
    else {
      if (state_ == kStateGameEnded) {
        // Stats live on the shadow (see setupShadowGame); finalize there
        // so gameTime / lifeSpans reflect the confirmed timeline.
        Game* sg = StatsGame();
        sg->stats_recorder->Finish(*sg);
      }
      return false;
    }
  } else {
    if (fadeValue_ < 33) fadeValue_ += 1;
  }

  return true;
}

bool RollbackController::WeaponSelectStep(uint8_t cur_local, uint8_t cur_remote) {
  uint8_t rising_local = cur_local & ~localPrevInput_;
  uint8_t rising_remote = cur_remote & ~remotePrevInput_;
  uint8_t released_local = localPrevInput_ & ~cur_local;
  uint8_t released_remote = remotePrevInput_ & ~cur_remote;

  game.worms[localIdx_]->control_states.istate |= rising_local;
  game.worms[remoteIdx_]->control_states.istate |= rising_remote;
  game.worms[localIdx_]->control_states.istate &= ~released_local;
  game.worms[remoteIdx_]->control_states.istate &= ~released_remote;

  for (int bit = 0; bit < 7; ++bit) {
    uint8_t mask = 1 << bit;
    if (rising_local & mask) {
      localHeldFrames_[bit] = 0;
    } else if (cur_local & mask) {
      ++localHeldFrames_[bit];
      if (localHeldFrames_[bit] >= kKeyRepeatInitial &&
          (localHeldFrames_[bit] - kKeyRepeatInitial) % kKeyRepeatInterval == 0) {
        game.worms[localIdx_]->control_states.istate |= mask;
      }
    } else {
      localHeldFrames_[bit] = 0;
    }
    if (rising_remote & mask) {
      remoteHeldFrames_[bit] = 0;
    } else if (cur_remote & mask) {
      ++remoteHeldFrames_[bit];
      if (remoteHeldFrames_[bit] >= kKeyRepeatInitial &&
          (remoteHeldFrames_[bit] - kKeyRepeatInitial) % kKeyRepeatInterval == 0) {
        game.worms[remoteIdx_]->control_states.istate |= mask;
      }
    } else {
      remoteHeldFrames_[bit] = 0;
    }
  }

  localPrevInput_ = cur_local;
  remotePrevInput_ = cur_remote;

  return ws_->ProcessFrame();
}

void RollbackController::SaveWeaponSelectSnap(WeaponSelectSnap& snap) const {
  snap.valid = true;
  for (int i = 0; i < 2; ++i) {
    Worm const& w = *game.worms[i];
    WormSettings const& ws_cfg = *w.settings;
    auto& p = snap.players[i];
    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      p.weapons[j] = ws_cfg.weapons[j];
    }
    p.is_ready = ws_->is_ready[i];
    p.menu_selection = ws_->menus[i].Selection();
    p.menu_top_item = ws_->menus[i].top_item;
    p.menu_bottom_item = ws_->menus[i].bottom_item;
    p.worm_control_states = static_cast<uint16_t>(w.control_states.istate);
    p.current_weapon = w.current_weapon;
  }
  snap.rand = game.rand;
  snap.local_prev_input = localPrevInput_;
  snap.remote_prev_input = remotePrevInput_;
  snap.local_held_frames = localHeldFrames_;
  snap.remote_held_frames = remoteHeldFrames_;
}

void RollbackController::LoadWeaponSelectSnap(WeaponSelectSnap const& snap) {
  Common const& common = *game.common;
  for (int i = 0; i < 2; ++i) {
    Worm& w = *game.worms[i];
    WormSettings& ws_cfg = *w.settings;
    auto const& p = snap.players[i];
    for (int j = 0; j < Settings::kSelectableWeapons; ++j) {
      ws_cfg.weapons[j] = p.weapons[j];
      int weap_order_idx = static_cast<int>(p.weapons[j]) - 1;
      if (weap_order_idx >= 0 && weap_order_idx < static_cast<int>(common.weap_order.size())) {
        int w_idx = common.weap_order[weap_order_idx];
        w.weapons[j].type = &common.weapons[w_idx];
        // menus[i].items index 0 is "Randomize", indices [1..N] are the
        // weapon slots, index N+1 is "Done".
        if (j + 1 < static_cast<int>(ws_->menus[i].items.size())) {
          ws_->menus[i].items[j + 1].string = common.weapons[w_idx].name;
        }
      }
    }
    ws_->is_ready[i] = p.is_ready;
    ws_->menus[i].SetSelection(p.menu_selection);
    ws_->menus[i].top_item = p.menu_top_item;
    ws_->menus[i].bottom_item = p.menu_bottom_item;
    w.control_states.istate = p.worm_control_states;
    w.current_weapon = p.current_weapon;
  }
  game.rand = snap.rand;
  localPrevInput_ = snap.local_prev_input;
  remotePrevInput_ = snap.remote_prev_input;
  localHeldFrames_ = snap.local_held_frames;
  remoteHeldFrames_ = snap.remote_held_frames;
}

void RollbackController::ResetForGamePhase() {
  localInputs_.fill(0);
  remoteInputs_.fill(0);
  remoteInputReady_.fill(false);

  simFrame_ = 0;
  confirmedSimFrame_ = -1;
  lastSentFrame_ = 0;
  lastSentFrameValid_ = false;
  lastRemoteInput_ = 0;
  lastKnownRemoteFrame_ = -1;

  // Edge-detection state — carrying these across the phase boundary
  // would produce a spurious rising/released edge on the first frame.
  localPrevInput_ = 0;
  remotePrevInput_ = 0;
  localHeldFrames_.fill(0);
  remoteHeldFrames_.fill(0);

  rollbackBuffer_.Clear();
  lastTickResimFrames_ = 0;

  // The generation bump is what makes the wire layer drop any
  // pre-transition batches still in flight from the peer.
  // generation_ is uint8_t and wraps. After 256 WS->game transitions an
  // ancient stale batch could collide with generation_+1 in
  // injectRemoteBatch; not reachable in any real match (one WS phase per
  // round) but worth knowing if this ever gets wired into a longer-lived
  // session.
  ++generation_;

  // Drain anything we buffered while the peer ran ahead. Re-feeding
  // through the normal path populates remoteInputs/remoteInputReady so
  // the first game-phase tick can advance confirmed instead of starving.
  uint8_t pending = pendingFutureCount_;
  pendingFutureCount_ = 0;
  for (uint8_t i = 0; i < pending; ++i) {
    auto const& s = pendingFutureBatches_[i];
    InjectRemoteBatch(generation_, s.base_frame, s.count, s.inputs.data(), s.remote_local_frame);
  }
}

void RollbackController::FinishWeaponSelect() {
  if (state_ != kStateWeaponSelection) return;

  ws_->Finalize();
  ws_.reset();

  for (auto const& w : game.worms) {
    w->lives = game.settings->lives;
  }
  game.StartGame();
  game.ResetWorms();
  state_ = kStateGame;

  // The WS phase can leave peers at different simFrame counters
  // (asymmetric stalls + WS-rollback resims). Carrying that skew into
  // game phase would silently diverge every simFrame-keyed comparison
  // downstream (checksums, terrain destruction) and trip the desync
  // detector. Resetting here gives the game phase a symmetric baseline.
  ResetForGamePhase();

  SeedRollbackAndShadow();
}

void RollbackController::SeedRollbackAndShadow() {
  // Game state vectors are fully sized only after startGame; prepare
  // here so the slot-0 snapshot below captures the right widths.
  if (!rollbackBufferPrepared_) {
    rollbackBuffer_.Prepare(game);
    rollbackBufferPrepared_ = true;
  }

  // Seed slot[0] = post-startGame state so a misprediction on the first
  // game-phase frame has a valid rollback target, and so setupShadowGame
  // has a frame-0 snapshot to clone into the shadow. The first
  // process() tick will overwrite this with the post-frame-0 snapshot.
  rollback::Slot& seed = rollbackBuffer_.Write(0);
  seed.local_input = 0;
  seed.remote_input = 0;
  seed.remote_state = rollback::RemoteState::kConfirmed;
  seed.ws_snap.valid = false;
  game.SaveSnapshotFast(seed.snapshot);
  seed.checksum = WideRollbackChecksum(game);

  SetupShadowGame();
}

void RollbackController::SetupShadowGame() {
  // Mirror the live game's construction: same Common/Settings, silent
  // sound player, identical worm+viewport configuration so the
  // snapshot we load below produces an identical processFrame path.
  shadowGame_ =
      std::make_unique<Game>(game.common, game.settings, std::make_shared<NullSoundPlayer>());

  ConfigureGameSlots(*shadowGame_, {game.worms[0]->settings, game.worms[1]->settings});

  // loadSnapshotFast assumes level buffers are already sized; the
  // snapshot itself carries pixel data but not dimensions.
  shadowGame_->level.width = game.level.width;
  shadowGame_->level.height = game.level.height;
  std::size_t cells =
      static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);
  shadowGame_->level.data.resize(cells);
  shadowGame_->level.materials.resize(cells);
  // origpal is the level's palette. It isn't sim state, so the fast
  // snapshot doesn't carry it — but the cereal Game serializer used by
  // ReplayWriter::beginRecord does. Without this copy the recorded
  // file embeds a zeroed palette and playback renders garbage colors.
  shadowGame_->level.origpal = game.level.origpal;

  // Match the post-WS setup the live controller did in
  // finishWeaponSelect: initWeapons before startGame, then startGame
  // and resetWorms. The snapshot load below overwrites the
  // simulation-visible side effects (rand, holdazone, worm pos), but
  // we still need startGame() to resize bobjects to bloodParticleMax
  // since the snapshot only carries `count`, not capacity.
  for (auto const& w : shadowGame_->worms) w->InitWeapons(*shadowGame_);
  shadowGame_->paused = false;
  shadowGame_->StartGame();
  shadowGame_->ResetWorms();

  // Frame-0 seed: live game's post-startGame state.
  if (auto* seed_slot = rollbackBuffer_.Find(0)) {
    shadowGame_->LoadSnapshotFast(seed_slot->snapshot);
  }

  shadowFrame_ = -1;
  shadowLocalPrevInput_ = 0;
  shadowRemotePrevInput_ = 0;
  shadowMismatchLogged_ = false;

  // The live game runs each frame speculatively (forward predicted +
  // resims), so its NormalStatsRecorder would over-count and — worse —
  // never increment frame on the peer whose forward path always falls
  // into the predicted branch. Stats accumulate exclusively on the
  // shadow, which advances once per confirmed frame. Replace the live
  // recorder with the base no-op so the in-sim hooks (damageDealt, hit,
  // afterSpawn, …) become silent on the live side.
  game.stats_recorder.reset(new StatsRecorder);

  StartReplayRecording();
}

void RollbackController::StartReplayRecording() {
  if (!shadowGame_) return;

  // Test path: caller provided a writer directly; skip the gfx-driven
  // file-naming logic and just hand it to ReplayWriter.
  if (replayWriterOverride_) {
    try {
      shadowReplay_ = std::make_unique<ReplayWriter>(std::move(replayWriterOverride_));
      shadowReplay_->BeginRecord(*shadowGame_);
    } catch (std::runtime_error& e) {
      std::fprintf(stderr, "[replay] failed to start recording: %s\n", e.what());
      shadowReplay_.reset();
    }
    return;
  }

  if (!Settings::kExtensions || !game.settings->record_replays) return;

  // Tests construct RollbackControllers without first wiring up gfx,
  // so getUserConfigNode() returns a default-constructed FsNode with
  // a null impl pointer. Operator/ would dereference that and segv.
  FsNode config_root = gfx.GetUserConfigNode();
  if (!config_root.imp) return;

  try {
    std::time_t ticks = std::time(nullptr);
    std::tm* now = std::localtime(&ticks);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H.%M.%S", now);

    std::string player_names = " ";
    for (std::size_t i = 0; i < shadowGame_->worms.size() && i < 2; ++i) {
      Worm& worm = *shadowGame_->worms[i];
      std::string const& name = worm.settings->name;
      if (i > 0) player_names.push_back('-');
      int chars = 0;
      for (std::size_t c = 0; c < name.size() && chars < 4; ++c, ++chars) {
        unsigned char ch = static_cast<unsigned char>(name[c]);
        if (std::isalnum(ch)) player_names.push_back(static_cast<char>(ch));
      }
    }

    // The " mp{idx}" suffix distinguishes the two peers' recordings if
    // they share a config directory and prevents the host's and
    // client's files from colliding.
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), " mp%d", localIdx_);

    auto node = config_root / "Replays" / (std::string(time_buf) + player_names + suffix + ".lrp");

    shadowReplay_ = std::make_unique<ReplayWriter>(node.ToWriter());
    shadowReplay_->BeginRecord(*shadowGame_);
  } catch (std::runtime_error& e) {
    std::fprintf(stderr, "[replay] failed to start recording: %s\n", e.what());
    shadowReplay_.reset();
  }
}

void RollbackController::StopReplayRecording() {
  // ReplayWriter destructor writes the 0x83 terminator.
  shadowReplay_.reset();
}

void RollbackController::DriveShadow() {
  if (!shadowGame_) return;

  while (shadowFrame_ < confirmedSimFrame_) {
    int32_t f = shadowFrame_ + 1;
    rollback::Slot const* slot = rollbackBuffer_.Find(f);
    if (!slot) {
      // The ring only holds kMaxRollback+1 slots; if the shadow ever
      // falls more than that behind, we can't reconstruct the missing
      // frame's inputs and have to give up on this match's recording.
      shadowGame_.reset();
      return;
    }

    uint8_t cur_local = (localIdx_ == 0) ? slot->local_input : slot->remote_input;
    uint8_t cur_remote = (localIdx_ == 0) ? slot->remote_input : slot->local_input;

    uint8_t rising_local = cur_local & ~shadowLocalPrevInput_;
    uint8_t rising_remote = cur_remote & ~shadowRemotePrevInput_;
    uint8_t released_local = shadowLocalPrevInput_ & ~cur_local;
    uint8_t released_remote = shadowRemotePrevInput_ & ~cur_remote;
    shadowGame_->worms[localIdx_]->control_states.istate |= rising_local;
    shadowGame_->worms[remoteIdx_]->control_states.istate |= rising_remote;
    shadowGame_->worms[localIdx_]->control_states.istate &= ~released_local;
    shadowGame_->worms[remoteIdx_]->control_states.istate &= ~released_remote;
    shadowLocalPrevInput_ = cur_local;
    shadowRemotePrevInput_ = cur_remote;

    // recordFrame() reads worm.controlStates ^ prevControlStates, so it
    // must run after edge detection but before processFrame() (which
    // sets prev = current at end of tick, wiping the delta).
    if (shadowReplay_) {
      try {
        shadowReplay_->RecordFrame();
      } catch (std::runtime_error& e) {
        std::fprintf(stderr, "[replay] aborting recording at frame %d: %s\n", f, e.what());
        shadowReplay_.reset();
      }
    }

    shadowGame_->ProcessFrame();
    shadowFrame_ = f;

    // Sanity check: the shadow must match the live game's confirmed
    // state for the same frame. Log once on divergence so the breakage
    // surfaces without spamming stderr.
    uint32_t shadow_chk = WideRollbackChecksum(*shadowGame_);
    if (shadow_chk != slot->checksum && !shadowMismatchLogged_) {
      std::fprintf(stderr,
                   "[replay shadow] mismatch at frame %d: shadow=%08x live=%08x"
                   " curLocal=%02x curRemote=%02x slot.localInput=%02x slot.remoteInput=%02x"
                   " shadowRand=%08x\n",
                   f, shadow_chk, slot->checksum, cur_local, cur_remote, slot->local_input,
                   slot->remote_input, shadowGame_->rand.last);
      shadowMismatchLogged_ = true;
    }
  }
}

// Mirror of advanceSimulation() for the weapon-select phase: same
// promote-confirmed-prefix → rollback-to-mismatch → resim-speculative →
// save-snapshot shape, but stepping weaponSelectStep instead of
// processFrame and saving into wsSnap instead of snapshot. Keep the two
// in sync — a fix here likely needs to land in the sibling too.
void RollbackController::AdvanceWeaponSelection() {
  uint32_t input_frame = simFrame_ + inputDelay_;
  if (!lastSentFrameValid_ || input_frame != lastSentFrame_) {
    uint32_t slot = input_frame % kInputBufferSize;
    // Mask to the 7 ControlState bits (Up/Down/Left/Right/Fire/Change/
    // Jump); bit 7 is reserved and must not leak onto the wire.
    localInputs_[slot] = localControlState_.Pack() & 0x7f;
    lastSentFrame_ = input_frame;
    lastSentFrameValid_ = true;
  }
  SendInputWindow(input_frame, simFrame_);

  // Promote previously-predicted WS frames whose real remote input has
  // now arrived and matches the prediction.
  int32_t rollback_to = -1;
  while (confirmedSimFrame_ + 1 < static_cast<int32_t>(simFrame_)) {
    int32_t f = confirmedSimFrame_ + 1;
    uint32_t s = static_cast<uint32_t>(f) % kInputBufferSize;
    if (!remoteInputReady_[s]) break;
    uint8_t real = remoteInputs_[s];

    auto* slot = rollbackBuffer_.Find(f);
    bool match = true;
    bool was_predicted = slot && slot->remote_state == rollback::RemoteState::kPredicted;
    if (was_predicted) {
      uint8_t stored_other = (localIdx_ == 0) ? slot->remote_input : slot->local_input;
      match = (stored_other == real);
    }

    if (!match) {
      rollback_to = f - 1;
      break;
    }

    if (slot) slot->remote_state = rollback::RemoteState::kConfirmed;
    lastRemoteInput_ = real;
    remoteInputReady_[s] = false;
    confirmedSimFrame_ = f;
  }

  // Rollback resim for the weapon-select phase. Identical structure to
  // advanceSimulation's resim, but with weaponSelectStep / wsSnap in
  // place of processFrame / GameSnapshot.
  if (rollback_to >= 0) {
    ++rollbackCount_;
    lastTickResimFrames_ +=
        static_cast<uint32_t>(static_cast<int32_t>(simFrame_) - rollback_to - 1);
    auto* last_good = rollbackBuffer_.Find(rollback_to);
    if (last_good && last_good->ws_snap.valid) {
      LoadWeaponSelectSnap(last_good->ws_snap);
    }

    uint8_t last_good_worm0 = last_good ? last_good->local_input : 0;
    uint8_t last_good_worm1 = last_good ? last_good->remote_input : 0;
    localPrevInput_ = (localIdx_ == 0) ? last_good_worm0 : last_good_worm1;
    remotePrevInput_ = (localIdx_ == 0) ? last_good_worm1 : last_good_worm0;

    game.SetSpeculative(true);
    for (int32_t f = rollback_to + 1; f < static_cast<int32_t>(simFrame_); ++f) {
      uint32_t s = static_cast<uint32_t>(f) % kInputBufferSize;
      uint8_t cur_local = localInputs_[s];
      uint8_t cur_remote;
      bool frame_predicted;
      bool resim_contiguous = (confirmedSimFrame_ + 1 == f);
      if (remoteInputReady_[s] && resim_contiguous) {
        cur_remote = remoteInputs_[s];
        remoteInputReady_[s] = false;
        lastRemoteInput_ = cur_remote;
        frame_predicted = false;
        confirmedSimFrame_ = f;
      } else {
        cur_remote = lastRemoteInput_;
        frame_predicted = true;
      }

      bool ws_done_resim = WeaponSelectStep(cur_local, cur_remote);

      auto& out_slot = rollbackBuffer_.Write(static_cast<int>(f));
      out_slot.local_input = (localIdx_ == 0) ? cur_local : cur_remote;
      out_slot.remote_input = (localIdx_ == 0) ? cur_remote : cur_local;
      out_slot.remote_state =
          frame_predicted ? rollback::RemoteState::kPredicted : rollback::RemoteState::kConfirmed;
      SaveWeaponSelectSnap(out_slot.ws_snap);
      out_slot.ws_snap.ws_done = ws_done_resim;
    }
    game.SetSpeculative(false);

    // Resim may have just confirmed the wsDone frame. Re-check here so
    // both peers transition on the same simFrame regardless of whether
    // wsDone was first observed forward or via a resim correcting a
    // mispredicted Fire press.
    if (rollback::Slot const* conf_slot = rollbackBuffer_.Find(confirmedSimFrame_)) {
      if (conf_slot->ws_snap.valid && conf_slot->ws_snap.ws_done) {
        FinishWeaponSelect();
        return;
      }
    }
  }

  // Stall guards — same thresholds as advanceSimulation.
  if (static_cast<int32_t>(simFrame_) - confirmedSimFrame_ > rollback::kMaxRollback) {
    return;
  }
  if (lastKnownRemoteFrame_ >= 0 &&
      static_cast<int32_t>(simFrame_) - lastKnownRemoteFrame_ >= frameAdvantageThreshold_) {
    ++frameAdvantageStalls_;
    return;
  }

  // Predict if remote input not yet ready, run ws tick, snapshot.
  uint32_t current_slot = simFrame_ % kInputBufferSize;
  uint8_t cur_local = localInputs_[current_slot];
  uint8_t cur_remote;
  bool predicted;
  bool chain_contiguous = confirmedSimFrame_ + 1 == static_cast<int32_t>(simFrame_);
  if (remoteInputReady_[current_slot] && chain_contiguous) {
    cur_remote = remoteInputs_[current_slot];
    remoteInputReady_[current_slot] = false;
    lastRemoteInput_ = cur_remote;
    predicted = false;
  } else {
    cur_remote = lastRemoteInput_;
    predicted = true;
  }

  game.SetSpeculative(predicted);
  bool ws_done = WeaponSelectStep(cur_local, cur_remote);
  game.SetSpeculative(false);
  ++simFrame_;

  if (!predicted) {
    confirmedSimFrame_ = static_cast<int32_t>(simFrame_) - 1;
  }

  // Snapshot post-frame weapon-select state.
  {
    int snap_frame = static_cast<int>(simFrame_) - 1;
    rollback::Slot& slot = rollbackBuffer_.Write(snap_frame);
    slot.local_input = (localIdx_ == 0) ? cur_local : cur_remote;
    slot.remote_input = (localIdx_ == 0) ? cur_remote : cur_local;
    slot.remote_state =
        predicted ? rollback::RemoteState::kPredicted : rollback::RemoteState::kConfirmed;
    SaveWeaponSelectSnap(slot.ws_snap);
    slot.ws_snap.ws_done = ws_done;
  }

  // Transition into game phase as soon as the highest confirmed frame's
  // snapshot shows wsDone=true. Gating on the confirmed slot (rather
  // than the current tick's possibly-predicted wsDone) means a forward
  // confirm, a promote-loop confirm, or a resim-confirm all fire the
  // transition on the same simFrame on both peers. Predicted wsDone
  // snapshots never fire it.
  if (rollback::Slot const* conf_slot = rollbackBuffer_.Find(confirmedSimFrame_)) {
    if (conf_slot->ws_snap.valid && conf_slot->ws_snap.ws_done) {
      FinishWeaponSelect();
    }
  }
}

// Mirror of advanceWeaponSelection() for the game phase. Keep the two
// in sync — a fix here likely needs to land in the sibling too.
void RollbackController::AdvanceSimulation() {
  uint32_t input_frame = simFrame_ + inputDelay_;
  if (!lastSentFrameValid_ || input_frame != lastSentFrame_) {
    uint32_t slot = input_frame % kInputBufferSize;
    // Mask to the 7 ControlState bits (Up/Down/Left/Right/Fire/Change/
    // Jump); bit 7 is reserved and must not leak onto the wire.
    localInputs_[slot] = localControlState_.Pack() & 0x7f;
    lastSentFrame_ = input_frame;
    lastSentFrameValid_ = true;
  }

  // Emit the last K = kMaxRollback + 1 local inputs as a redundant
  // batch every tick. The redundancy covers single dropped packets
  // without a retransmit RTT. Send continues even when stalled below so
  // the remote peer can promote out of its own stall.
  SendInputWindow(input_frame, simFrame_);

  // Walk confirmedSimFrame_+1 .. simFrame-1 in order: promote slots whose
  // prediction matched, stop at the first mismatch and let the resim
  // below reload its predecessor's snapshot.
  int32_t rollback_to = -1;
  while (confirmedSimFrame_ + 1 < static_cast<int32_t>(simFrame_)) {
    int32_t f = confirmedSimFrame_ + 1;
    uint32_t s = static_cast<uint32_t>(f) % kInputBufferSize;
    if (!remoteInputReady_[s]) break;
    uint8_t real = remoteInputs_[s];

    auto* slot = rollbackBuffer_.Find(f);
    bool match = true;
    bool was_predicted = slot && slot->remote_state == rollback::RemoteState::kPredicted;
    if (was_predicted) {
      // slot stores inputs by worm index: localInput=worm0, remoteInput=worm1.
      // The misprediction question is about the *other* peer's input.
      uint8_t stored_other = (localIdx_ == 0) ? slot->remote_input : slot->local_input;
      match = (stored_other == real);
    }

    if (!match) {
      rollback_to = f - 1;
      break;
    }

    if (slot) slot->remote_state = rollback::RemoteState::kConfirmed;
    lastRemoteInput_ = real;
    remoteInputReady_[s] = false;
    confirmedSimFrame_ = f;

    // The forward path skipped sending a checksum when the frame was
    // first run as predicted; emit the cached value now that we've
    // confirmed it.
    if (was_predicted && sendChecksum_) {
      sendChecksum_(generation_, static_cast<uint32_t>(f), slot->checksum);
    }
  }

  // Load the last known-good snapshot, then replay every frame after it
  // with the freshest input available. Speculative across the window so
  // previously-emitted sounds/stats don't duplicate.
  if (rollback_to >= 0) {
    ++rollbackCount_;
    lastTickResimFrames_ +=
        static_cast<uint32_t>(static_cast<int32_t>(simFrame_) - rollback_to - 1);
    auto* last_good = rollbackBuffer_.Find(rollback_to);
    // Resident by construction: the stall guard caps simFrame - confirmedSimFrame_
    // at kMaxRollback, and the ring holds kMaxRollback+1 slots.
    game.LoadSnapshotFast(last_good->snapshot);

    uint8_t last_good_worm0 = last_good->local_input;
    uint8_t last_good_worm1 = last_good->remote_input;
    localPrevInput_ = (localIdx_ == 0) ? last_good_worm0 : last_good_worm1;
    remotePrevInput_ = (localIdx_ == 0) ? last_good_worm1 : last_good_worm0;

    game.SetSpeculative(true);
    for (int32_t f = rollback_to + 1; f < static_cast<int32_t>(simFrame_); ++f) {
      uint32_t s = static_cast<uint32_t>(f) % kInputBufferSize;
      uint8_t cur_local = localInputs_[s];
      uint8_t cur_remote;
      bool frame_predicted;
      // Same contiguity rule as the new-frame block — only consume real
      // input (and clear the ring entry) when the chain reaches F-1.
      // Out-of-order real inputs stay in the ring for a later promote.
      bool resim_contiguous = (confirmedSimFrame_ + 1 == f);
      if (remoteInputReady_[s] && resim_contiguous) {
        cur_remote = remoteInputs_[s];
        remoteInputReady_[s] = false;
        lastRemoteInput_ = cur_remote;
        frame_predicted = false;
        confirmedSimFrame_ = f;
      } else {
        cur_remote = lastRemoteInput_;
        frame_predicted = true;
      }

      uint8_t rising_local = cur_local & ~localPrevInput_;
      uint8_t rising_remote = cur_remote & ~remotePrevInput_;
      uint8_t released_local = localPrevInput_ & ~cur_local;
      uint8_t released_remote = remotePrevInput_ & ~cur_remote;
      game.worms[localIdx_]->control_states.istate |= rising_local;
      game.worms[remoteIdx_]->control_states.istate |= rising_remote;
      game.worms[localIdx_]->control_states.istate &= ~released_local;
      game.worms[remoteIdx_]->control_states.istate &= ~released_remote;
      localPrevInput_ = cur_local;
      remotePrevInput_ = cur_remote;

      game.ProcessFrame();

      auto& out_slot = rollbackBuffer_.Write(static_cast<int>(f));
      out_slot.local_input = (localIdx_ == 0) ? cur_local : cur_remote;
      out_slot.remote_input = (localIdx_ == 0) ? cur_remote : cur_local;
      out_slot.remote_state =
          frame_predicted ? rollback::RemoteState::kPredicted : rollback::RemoteState::kConfirmed;
      game.SaveSnapshotFast(out_slot.snapshot);
      out_slot.checksum = WideRollbackChecksum(game);

      // Predicted resim frames stay silent — their checksum is cached
      // for a later promote/resim pass.
      if (!frame_predicted && sendChecksum_) {
        sendChecksum_(generation_, static_cast<uint32_t>(f), out_slot.checksum);
      }
    }
    game.SetSpeculative(false);
  }

  // Stall guard: cap in-flight predicted frames at kMaxRollback so the
  // ring buffer (kMaxRollback+1 slots) can still cover the post-tick
  // window.
  if (static_cast<int32_t>(simFrame_) - confirmedSimFrame_ > rollback::kMaxRollback) {
    return;
  }

  // Frame-advantage stall: hold simFrame when too far ahead of the
  // remote's last reported simFrame. The send above already ran, so the
  // remote still hears from us this tick. -1 keeps this disarmed before
  // any packet has arrived.
  if (lastKnownRemoteFrame_ >= 0 &&
      static_cast<int32_t>(simFrame_) - lastKnownRemoteFrame_ >= frameAdvantageThreshold_) {
    ++frameAdvantageStalls_;
    return;
  }

  uint32_t current_slot = simFrame_ % kInputBufferSize;

  uint8_t cur_local = localInputs_[current_slot];
  uint8_t cur_remote;
  bool predicted;
  // Only consume real remote input when the confirmed chain reaches
  // simFrame-1. Out-of-order arrival (real input for a future frame
  // while an earlier one is still missing) must stay in the ring so a
  // later promote can fold it into a contiguous chain.
  bool chain_contiguous = confirmedSimFrame_ + 1 == static_cast<int32_t>(simFrame_);
  if (remoteInputReady_[current_slot] && chain_contiguous) {
    cur_remote = remoteInputs_[current_slot];
    remoteInputReady_[current_slot] = false;
    lastRemoteInput_ = cur_remote;
    predicted = false;
  } else {
    cur_remote = lastRemoteInput_;
    predicted = true;
  }

  uint8_t rising_local = cur_local & ~localPrevInput_;
  uint8_t rising_remote = cur_remote & ~remotePrevInput_;
  uint8_t released_local = localPrevInput_ & ~cur_local;
  uint8_t released_remote = remotePrevInput_ & ~cur_remote;

  game.worms[localIdx_]->control_states.istate |= rising_local;
  game.worms[remoteIdx_]->control_states.istate |= rising_remote;
  game.worms[localIdx_]->control_states.istate &= ~released_local;
  game.worms[remoteIdx_]->control_states.istate &= ~released_remote;

  localPrevInput_ = cur_local;
  remotePrevInput_ = cur_remote;

  game.SetSpeculative(predicted);
  game.ProcessFrame();
  game.SetSpeculative(false);
  ++simFrame_;

  if (!predicted) {
    confirmedSimFrame_ = static_cast<int32_t>(simFrame_) - 1;
  }

  // Snapshot the post-frame state and cache the checksum (used by a
  // later promote/resim if the frame was predicted).
  {
    int snap_frame = static_cast<int>(simFrame_) - 1;
    rollback::Slot& slot = rollbackBuffer_.Write(snap_frame);
    slot.local_input = (localIdx_ == 0) ? cur_local : cur_remote;
    slot.remote_input = (localIdx_ == 0) ? cur_remote : cur_local;
    slot.remote_state =
        predicted ? rollback::RemoteState::kPredicted : rollback::RemoteState::kConfirmed;
    game.SaveSnapshotFast(slot.snapshot);
    slot.checksum = WideRollbackChecksum(game);

    if (!predicted && sendChecksum_) {
      sendChecksum_(generation_, simFrame_ - 1, slot.checksum);
    }
  }

  if (game.IsGameOver()) {
    state_ = kStateGameEnded;
    if (!goingToMenu_) EnterGoingToMenu(180);
  }

  DriveShadow();
}

void RollbackController::Draw(Renderer& renderer, bool use_spectator_viewports) {
  if (state_ == kStateWeaponSelection) {
    ws_->Draw(renderer, state_, use_spectator_viewports);
  } else if (state_ == kStateGame || state_ == kStateGameEnded) {
    game.Draw(renderer, state_, use_spectator_viewports);
  }
  renderer.fade_value = fadeValue_;

  // Dev HUD: bottom-left `RB:n` resim indicator, always shown so the
  // value doesn't blink as resim windows come and go.
  if (state_ == kStateGame) {
    Font& font = game.common->font;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "RB:%u", lastTickResimFrames_);
    font.DrawString(renderer.bmp, buf, 2, renderer.render_res_y - 9, 50);
  }

  if (IsPaused()) {
    Fill(renderer.bmp, 0);
    Common& common = *game.common;
    Font& font = common.font;
    int cx = renderer.render_res_x / 2;
    int cy = renderer.render_res_y / 2 - 20;

    renderer.pal = game.common->exepal;
    renderer.pal.RotateFrom(game.common->exepal, 168, 174, gfx.menu_cycles);
    renderer.pal.Fade(fadeValue_);

    if (localPaused_) {
      std::string title = "GAME PAUSED";
      int tw = font.GetDims(title);
      font.DrawString(renderer.bmp, title, cx - tw / 2, cy, 50);

      pauseMenu_.Place(cx, cy + 16);
      pauseMenu_.Draw(common, renderer, false);
    } else {
      std::string title = "PAUSED BY PEER";
      int tw = font.GetDims(title);
      font.DrawString(renderer.bmp, title, cx - tw / 2, cy, 50);

      std::string hint = "PRESS ESC TO DISCONNECT";
      int hw = font.GetDims(hint);
      font.DrawString(renderer.bmp, hint, cx - hw / 2, cy + 16, 6);
    }
  }
}

void RollbackController::SwapLevel(Level& new_level) { CurrentLevel()->Swap(new_level); }

Level* RollbackController::CurrentLevel() { return &game.level; }

Game* RollbackController::CurrentGame() { return &game; }

Game* RollbackController::StatsGame() { return shadowGame_ ? shadowGame_.get() : &game; }

bool RollbackController::Running() {
  return state_ != kStateGameEnded && state_ != kStateInitial && resumable_;
}

void RollbackController::EnterGoingToMenu(int fade) {
  goingToMenu_ = true;
  fadeValue_ = fade;
  // Without this, process()'s isPaused() early return would skip the
  // fade decrement and the menu transition would stall.
  localPaused_ = false;
  remotePaused_ = false;
}

void RollbackController::EndMatch() {
  if (state_ == kStateGame || state_ == kStateWeaponSelection) {
    state_ = kStateGameEnded;
    EnterGoingToMenu(33);
    StopReplayRecording();
  }
}

void RollbackController::PeerLeft() {
  // Mirrors the local Disconnect pause-menu branch: skip the fade and
  // do NOT transition to StateGameEnded — that keeps statsRecorder
  // unfinalized so the host loop routes us back to the menu rather
  // than to the stats screen.
  EnterGoingToMenu(0);
  resumable_ = false;
  StopReplayRecording();
}
