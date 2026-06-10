#include "rollbackController.hpp"

#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../replay.hpp"
#include "../spectatorviewport.hpp"
#include "../viewport.hpp"

#include <miniz.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <utility>

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

RollbackController::RollbackController(const std::shared_ptr<Common>& common,
                                       const std::shared_ptr<Settings>& settings,
                                       int local_player_idx)
    : game(common, settings, gfx.sound_player),
      localIdx_(local_player_idx),
      remoteIdx_(local_player_idx ^ 1) {
  localHeldFrames_.fill(0);
  remoteHeldFrames_.fill(0);

  pauseMenu_.Init(/*centered_init=*/true);
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

RollbackController::~RollbackController() = default;

void RollbackController::LoadLevelFromData(const std::vector<uint8_t>& data) {
  if (data.size() < 5) {
    return;
  }

  bool const kIsCompressed = (data[0] != 0);
  uint32_t raw_size = 0;
  std::memcpy(&raw_size, data.data() + 1, 4);

  static constexpr uint32_t kMaxRawSize = 10 * 1024 * 1024;
  if (raw_size > kMaxRawSize) {
    return;
  }

  std::vector<uint8_t> raw;
  if (kIsCompressed) {
    raw.resize(raw_size);
    mz_ulong dest_len = raw_size;
    int const kStatus = mz_uncompress(raw.data(), &dest_len, data.data() + 5,
                                      static_cast<mz_ulong>(data.size() - 5));
    if (kStatus != MZ_OK) {
      return;
    }
  } else {
    raw.assign(data.begin() + 5, data.end());
  }

  if (raw.size() < 8) {
    return;
  }

  uint16_t w = 0;
  uint16_t h = 0;
  std::memcpy(&w, raw.data(), 2);
  std::memcpy(&h, raw.data() + 2, 2);

  if (w == 0 || h == 0 || w > 4096 || h > 4096) {
    return;
  }

  uint32_t rand_state_len = 0;
  std::memcpy(&rand_state_len, raw.data() + 4, 4);

  if (rand_state_len > 64 * 1024) {
    return;
  }
  if (raw.size() < 8 + rand_state_len + 4) {
    return;
  }

  std::string const kRandState(reinterpret_cast<const char*>(raw.data() + 8), rand_state_len);
  uint32_t rand_last = 0;
  std::memcpy(&rand_last, raw.data() + 8 + rand_state_len, 4);

  size_t const kPixelsOffset = 8 + rand_state_len + 4;
  size_t const kPixelDataSize = static_cast<size_t>(w) * h;
  if (raw.size() < kPixelsOffset + kPixelDataSize + 768) {
    return;
  }

  game.level.Resize(w, h);
  Common const& common = *game.common;

  const uint8_t* pixels = raw.data() + kPixelsOffset;
  for (size_t i = 0; i < kPixelDataSize; ++i) {
    game.level.data[i] = pixels[i];
    game.level.materials[i] = common.materials[pixels[i]];
  }

  const uint8_t* pal_data = raw.data() + kPixelsOffset + kPixelDataSize;
  for (int i = 0; i < 256; ++i) {
    game.level.origpal.entries[i].r = pal_data[i * 3 + 0];
    game.level.origpal.entries[i].g = pal_data[i * 3 + 1];
    game.level.origpal.entries[i].b = pal_data[i * 3 + 2];
  }

  game.rand.Deserialize(kRandState);
  game.rand.last = rand_last;

  levelPreloaded_ = true;
}

void RollbackController::SetInputCallbacks(InputBatchSendCallback send) {
  sendInputBatch_ = std::move(send);
}

void RollbackController::SendInputWindow(uint32_t newest_frame, uint32_t local_frame) {
  if (!sendInputBatch_) {
    return;
  }
  constexpr auto kK = static_cast<uint8_t>(rollback::kMaxRollback + 1);
  uint8_t count = 0;
  uint32_t base_frame = 0;
  if (newest_frame + 1U < kK) {
    count = static_cast<uint8_t>(newest_frame + 1U);
    base_frame = 0;
  } else {
    count = kK;
    base_frame = newest_frame - (kK - 1U);
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
    auto const kF = static_cast<int32_t>(remote_local_frame);
    lastKnownRemoteFrame_ = std::max(kF, lastKnownRemoteFrame_);
    return;
  }

  if (generation == static_cast<uint8_t>(generation_ + 1) && count <= kMaxPendingFutureBatches) {
    if (pendingFutureCount_ < kMaxPendingFutureBatches) {
      auto& slot = pendingFutureBatches_[pendingFutureCount_++];
      slot.base_frame = base_frame;
      slot.count = count;
      for (uint8_t i = 0; i < count; ++i) {
        slot.inputs[i] = inputs[i];
      }
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
  if (std::cmp_less_equal(frame, confirmedSimFrame_)) {
    return;
  }
  uint32_t const kSlot = frame % kInputBufferSize;
  remoteInputs_[kSlot] = input;
  remoteInputReady_[kSlot] = true;
}

void RollbackController::OnKey(int key, bool key_state) {
  Worm::Control control{};
  Worm* worm = game.worms[localIdx_].get();
  bool found = false;

  if (worm->settings->input_device == WormSettingsExtensions::kInputKeyboard) {
    uint32_t const* controls =
        Settings::kExtensions ? worm->settings->controls_ex : worm->settings->controls;
    std::size_t const kMaxControl =
        Settings::kExtensions ? WormSettings::kMaxControlEx : WormSettings::kMaxControl;
    for (std::size_t c = 0; c < kMaxControl; ++c) {
      if (std::cmp_equal(controls[c], key)) {
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
      localControlState_.Set(Worm::kLeft, /*v=*/true);
      localControlState_.Set(Worm::kRight, /*v=*/true);
    } else {
      if (!worm->clean_control_states[Worm::kLeft]) {
        localControlState_.Set(Worm::kLeft, /*v=*/false);
      }
      if (!worm->clean_control_states[Worm::kRight]) {
        localControlState_.Set(Worm::kRight, /*v=*/false);
      }
    }
  }

  if (key == kDkEscape && key_state) {
    if (localPaused_) {
      localPaused_ = false;
      if (onLocalResume_) {
        onLocalResume_();
      }
    } else if (remotePaused_ && !goingToMenu_) {
      // Remote is paused, local Escapes — treat as a disconnect so the
      // peer learns and tears down in lockstep instead of waiting for
      // socket timeout.
      if (onPeerLeft_) {
        onPeerLeft_();
      }
      PeerLeft();
    } else if (!goingToMenu_) {
      localPaused_ = true;
      pauseMenu_.MoveToFirstVisible();
      if (onLocalPause_) {
        onLocalPause_();
      }
    }
  }
}

void RollbackController::Unfocus() {
  if (state_ == kStateWeaponSelection && ws_) {
    ws_->Unfocus();
  }
  localPaused_ = false;
  remotePaused_ = false;
}

void RollbackController::Focus() {
  if (state_ == kStateGameEnded) {
    goingToMenu_ = true;
    fadeValue_ = 0;
    return;
  }
  if (state_ == kStateWeaponSelection) {
    ws_->Focus();
  }
  if (state_ == kStateInitial) {
    if (!levelPreloaded_) {
      game.level.GenerateFromSettings(*game.common, *game.settings, game.rand);
    }

    if (skipWeaponSelection_) {
      for (auto const& w : game.worms) {
        w->InitWeapons(game);
      }
      for (auto const& w : game.worms) {
        w->lives = game.settings->lives;
      }
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
    if (fadeValue_ < 33) {
      fadeValue_ += 1;
    }

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
        int const kSel = pauseMenu_.SelectedId();
        if (kSel == 0) {
          localPaused_ = false;
          if (onLocalResume_) {
            onLocalResume_();
          }
        } else if (kSel == 2) {
          localPaused_ = false;
          if (onLocalResume_) {
            onLocalResume_();
          }
          if (onEndMatch_) {
            onEndMatch_();
          }
          EndMatch();
        } else {
          if (onLocalResume_) {
            onLocalResume_();
          }
          if (onPeerLeft_) {
            onPeerLeft_();
          }
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
    if (fadeValue_ > 0) {
      fadeValue_ -= 1;
    } else {
      if (state_ == kStateGameEnded) {
        // Stats live on the shadow (see setupShadowGame); finalize there
        // so gameTime / lifeSpans reflect the confirmed timeline.
        Game* sg = StatsGame();
        sg->stats_recorder->Finish(*sg);
      }
      return false;
    }
  } else {
    if (fadeValue_ < 33) {
      fadeValue_ += 1;
    }
  }

  return true;
}

bool RollbackController::WeaponSelectStep(uint8_t cur_local, uint8_t cur_remote) {
  uint8_t const kRisingLocal = cur_local & ~localPrevInput_;
  uint8_t const kRisingRemote = cur_remote & ~remotePrevInput_;
  uint8_t const kReleasedLocal = localPrevInput_ & ~cur_local;
  uint8_t const kReleasedRemote = remotePrevInput_ & ~cur_remote;

  game.worms[localIdx_]->control_states.istate |= kRisingLocal;
  game.worms[remoteIdx_]->control_states.istate |= kRisingRemote;
  game.worms[localIdx_]->control_states.istate &= ~kReleasedLocal;
  game.worms[remoteIdx_]->control_states.istate &= ~kReleasedRemote;

  for (int bit = 0; bit < 7; ++bit) {
    uint8_t const kMask = 1 << bit;
    // NOLINTNEXTLINE(bugprone-branch-clone) — rising-edge and released branches both reset the counter; merging them obscures the state machine.
    if (kRisingLocal & kMask) {
      localHeldFrames_[bit] = 0;
    } else if (cur_local & kMask) {
      ++localHeldFrames_[bit];
      if (localHeldFrames_[bit] >= kKeyRepeatInitial &&
          (localHeldFrames_[bit] - kKeyRepeatInitial) % kKeyRepeatInterval == 0) {
        game.worms[localIdx_]->control_states.istate |= kMask;
      }
    } else {
      localHeldFrames_[bit] = 0;
    }
    // NOLINTNEXTLINE(bugprone-branch-clone) — same pattern as the local-side branch above.
    if (kRisingRemote & kMask) {
      remoteHeldFrames_[bit] = 0;
    } else if (cur_remote & kMask) {
      ++remoteHeldFrames_[bit];
      if (remoteHeldFrames_[bit] >= kKeyRepeatInitial &&
          (remoteHeldFrames_[bit] - kKeyRepeatInitial) % kKeyRepeatInterval == 0) {
        game.worms[remoteIdx_]->control_states.istate |= kMask;
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
      int const kWeapOrderIdx = static_cast<int>(p.weapons[j]) - 1;
      if (kWeapOrderIdx >= 0 && std::cmp_less(kWeapOrderIdx, common.weap_order.size())) {
        int const kWIdx = common.weap_order[kWeapOrderIdx];
        w.weapons[j].type = &common.weapons[kWIdx];
        // menus[i].items index 0 is "Randomize", indices [1..N] are the
        // weapon slots, index N+1 is "Done".
        if (j + 1 < static_cast<int>(ws_->menus[i].items.size())) {
          ws_->menus[i].items[j + 1].string = common.weapons[kWIdx].name;
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
  uint8_t const kPending = pendingFutureCount_;
  pendingFutureCount_ = 0;
  for (uint8_t i = 0; i < kPending; ++i) {
    auto const& s = pendingFutureBatches_[i];
    InjectRemoteBatch(generation_, s.base_frame, s.count, s.inputs.data(), s.remote_local_frame);
  }
}

void RollbackController::FinishWeaponSelect() {
  if (state_ != kStateWeaponSelection) {
    return;
  }

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
  // install_global_sound_player=false: the shadow must not replace
  // g_sound_player with its NullSoundPlayer — that would mute every
  // menu/UI sound for the rest of the match (and leave a dangling
  // global after a rematch cycle).
  shadowGame_ =
      std::make_unique<Game>(game.common, game.settings, std::make_shared<NullSoundPlayer>(),
                             /*install_global_sound_player=*/false);

  ConfigureGameSlots(*shadowGame_, {game.worms[0]->settings, game.worms[1]->settings});

  // loadSnapshotFast assumes level buffers are already sized; the
  // snapshot itself carries pixel data but not dimensions.
  shadowGame_->level.width = game.level.width;
  shadowGame_->level.height = game.level.height;
  std::size_t const kCells =
      static_cast<std::size_t>(game.level.width) * static_cast<std::size_t>(game.level.height);
  shadowGame_->level.data.resize(kCells);
  shadowGame_->level.materials.resize(kCells);
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
  for (auto const& w : shadowGame_->worms) {
    w->InitWeapons(*shadowGame_);
  }
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
  game.stats_recorder = std::make_shared<StatsRecorder>();

  StartReplayRecording();
}

void RollbackController::StartReplayRecording() {
  if (!shadowGame_) {
    return;
  }

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

  if (!Settings::kExtensions || !game.settings->record_replays) {
    return;
  }

  // Tests construct RollbackControllers without first wiring up gfx,
  // so getUserConfigNode() returns a default-constructed FsNode with
  // a null impl pointer. Operator/ would dereference that and segv.
  FsNode const kConfigRoot = gfx.GetUserConfigNode();
  if (!kConfigRoot.imp) {
    return;
  }

  try {
    std::time_t const kTicks = std::time(nullptr);
    std::tm* now = std::localtime(&kTicks);
    char time_buf[64];
    // NOLINTNEXTLINE(cert-err33-c) — buffer is generous; truncation only on a malformed locale and is non-fatal here.
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H.%M.%S", now);

    std::string player_names = " ";
    for (std::size_t i = 0; i < shadowGame_->worms.size() && i < 2; ++i) {
      Worm const& worm = *shadowGame_->worms[i];
      std::string const& name = worm.settings->name;
      if (i > 0) {
        player_names.push_back('-');
      }
      int chars = 0;
      for (std::size_t c = 0; c < name.size() && chars < 4; ++c, ++chars) {
        auto const kCh = static_cast<unsigned char>(name[c]);
        if (std::isalnum(kCh)) {
          player_names.push_back(static_cast<char>(kCh));
        }
      }
    }

    // The " mp{idx}" suffix distinguishes the two peers' recordings if
    // they share a config directory and prevents the host's and
    // client's files from colliding.
    char suffix[8];
    // NOLINTNEXTLINE(cert-err33-c) — fixed 7-char output ("mpN\0") fits the 8-byte buffer.
    std::snprintf(suffix, sizeof(suffix), " mp%d", localIdx_);

    auto node = kConfigRoot / "Replays" / (std::string(time_buf) + player_names + suffix + ".lrp");

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
  if (!shadowGame_) {
    return;
  }

  while (shadowFrame_ < confirmedSimFrame_) {
    int32_t const kF = shadowFrame_ + 1;
    rollback::Slot const* slot = rollbackBuffer_.Find(kF);
    if (!slot) {
      // The ring only holds kMaxRollback+1 slots; if the shadow ever
      // falls more than that behind, we can't reconstruct the missing
      // frame's inputs and have to give up on this match's recording.
      shadowGame_.reset();
      return;
    }

    uint8_t const kCurLocal = (localIdx_ == 0) ? slot->local_input : slot->remote_input;
    uint8_t const kCurRemote = (localIdx_ == 0) ? slot->remote_input : slot->local_input;

    uint8_t const kRisingLocal = kCurLocal & ~shadowLocalPrevInput_;
    uint8_t const kRisingRemote = kCurRemote & ~shadowRemotePrevInput_;
    uint8_t const kReleasedLocal = shadowLocalPrevInput_ & ~kCurLocal;
    uint8_t const kReleasedRemote = shadowRemotePrevInput_ & ~kCurRemote;
    shadowGame_->worms[localIdx_]->control_states.istate |= kRisingLocal;
    shadowGame_->worms[remoteIdx_]->control_states.istate |= kRisingRemote;
    shadowGame_->worms[localIdx_]->control_states.istate &= ~kReleasedLocal;
    shadowGame_->worms[remoteIdx_]->control_states.istate &= ~kReleasedRemote;
    shadowLocalPrevInput_ = kCurLocal;
    shadowRemotePrevInput_ = kCurRemote;

    // recordFrame() reads worm.controlStates ^ prevControlStates, so it
    // must run after edge detection but before processFrame() (which
    // sets prev = current at end of tick, wiping the delta).
    if (shadowReplay_) {
      try {
        shadowReplay_->RecordFrame();
      } catch (std::runtime_error& e) {
        std::fprintf(stderr, "[replay] aborting recording at frame %d: %s\n", kF, e.what());
        shadowReplay_.reset();
      }
    }

    shadowGame_->ProcessFrame();
    shadowFrame_ = kF;

    // Sanity check: the shadow must match the live game's confirmed
    // state for the same frame. Log once on divergence so the breakage
    // surfaces without spamming stderr.
    uint32_t const kShadowChk = WideRollbackChecksum(*shadowGame_);
    if (kShadowChk != slot->checksum && !shadowMismatchLogged_) {
      std::fprintf(stderr,
                   "[replay shadow] mismatch at frame %d: shadow=%08x live=%08x curLocal=%02x "
                   "curRemote=%02x slot.localInput=%02x slot.remoteInput=%02x shadowRand=%08x\n",
                   kF, kShadowChk, slot->checksum, kCurLocal, kCurRemote, slot->local_input,
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
  uint32_t const kInputFrame = simFrame_ + inputDelay_;
  if (!lastSentFrameValid_ || kInputFrame != lastSentFrame_) {
    uint32_t const kSlot = kInputFrame % kInputBufferSize;
    // Mask to the 7 ControlState bits (Up/Down/Left/Right/Fire/Change/
    // Jump); bit 7 is reserved and must not leak onto the wire.
    localInputs_[kSlot] = localControlState_.Pack() & 0x7f;
    lastSentFrame_ = kInputFrame;
    lastSentFrameValid_ = true;
  }
  SendInputWindow(kInputFrame, simFrame_);

  // Promote previously-predicted WS frames whose real remote input has
  // now arrived and matches the prediction.
  int32_t rollback_to = -1;
  while (confirmedSimFrame_ + 1 < static_cast<int32_t>(simFrame_)) {
    int32_t const kF = confirmedSimFrame_ + 1;
    uint32_t const kS = static_cast<uint32_t>(kF) % kInputBufferSize;
    if (!remoteInputReady_[kS]) {
      break;
    }
    uint8_t const kReal = remoteInputs_[kS];

    auto* slot = rollbackBuffer_.Find(kF);
    bool match = true;
    bool const kWasPredicted = slot && slot->remote_state == rollback::RemoteState::kPredicted;
    if (kWasPredicted) {
      uint8_t const kStoredOther = (localIdx_ == 0) ? slot->remote_input : slot->local_input;
      match = (kStoredOther == kReal);
    }

    if (!match) {
      rollback_to = kF - 1;
      break;
    }

    if (slot) {
      slot->remote_state = rollback::RemoteState::kConfirmed;
    }
    lastRemoteInput_ = kReal;
    remoteInputReady_[kS] = false;
    confirmedSimFrame_ = kF;
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

    uint8_t const kLastGoodWorm0 = last_good ? last_good->local_input : 0;
    uint8_t const kLastGoodWorm1 = last_good ? last_good->remote_input : 0;
    localPrevInput_ = (localIdx_ == 0) ? kLastGoodWorm0 : kLastGoodWorm1;
    remotePrevInput_ = (localIdx_ == 0) ? kLastGoodWorm1 : kLastGoodWorm0;

    game.SetSpeculative(/*s=*/true);
    for (int32_t f = rollback_to + 1; std::cmp_less(f, simFrame_); ++f) {
      uint32_t const kS = static_cast<uint32_t>(f) % kInputBufferSize;
      uint8_t const kCurLocal = localInputs_[kS];
      uint8_t cur_remote = 0;
      bool frame_predicted = false;
      bool const kResimContiguous = (confirmedSimFrame_ + 1 == f);
      if (remoteInputReady_[kS] && kResimContiguous) {
        cur_remote = remoteInputs_[kS];
        remoteInputReady_[kS] = false;
        lastRemoteInput_ = cur_remote;
        frame_predicted = false;
        confirmedSimFrame_ = f;
      } else {
        cur_remote = lastRemoteInput_;
        frame_predicted = true;
      }

      bool const kWsDoneResim = WeaponSelectStep(kCurLocal, cur_remote);

      auto& out_slot = rollbackBuffer_.Write(static_cast<int>(f));
      out_slot.local_input = (localIdx_ == 0) ? kCurLocal : cur_remote;
      out_slot.remote_input = (localIdx_ == 0) ? cur_remote : kCurLocal;
      out_slot.remote_state =
          frame_predicted ? rollback::RemoteState::kPredicted : rollback::RemoteState::kConfirmed;
      SaveWeaponSelectSnap(out_slot.ws_snap);
      out_slot.ws_snap.ws_done = kWsDoneResim;
    }
    game.SetSpeculative(/*s=*/false);

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
  uint32_t const kCurrentSlot = simFrame_ % kInputBufferSize;
  uint8_t const kCurLocal = localInputs_[kCurrentSlot];
  uint8_t cur_remote = 0;
  bool predicted = false;
  bool const kChainContiguous = confirmedSimFrame_ + 1 == static_cast<int32_t>(simFrame_);
  if (remoteInputReady_[kCurrentSlot] && kChainContiguous) {
    cur_remote = remoteInputs_[kCurrentSlot];
    remoteInputReady_[kCurrentSlot] = false;
    lastRemoteInput_ = cur_remote;
    predicted = false;
  } else {
    cur_remote = lastRemoteInput_;
    predicted = true;
  }

  // First execution runs audible even when predicted — see the matching
  // comment in advanceSimulation. Only resim re-executions are speculative.
  bool const kWsDone = WeaponSelectStep(kCurLocal, cur_remote);
  ++simFrame_;

  if (!predicted) {
    confirmedSimFrame_ = static_cast<int32_t>(simFrame_) - 1;
  }

  // Snapshot post-frame weapon-select state.
  {
    int const kSnapFrame = static_cast<int>(simFrame_) - 1;
    rollback::Slot& slot = rollbackBuffer_.Write(kSnapFrame);
    slot.local_input = (localIdx_ == 0) ? kCurLocal : cur_remote;
    slot.remote_input = (localIdx_ == 0) ? cur_remote : kCurLocal;
    slot.remote_state =
        predicted ? rollback::RemoteState::kPredicted : rollback::RemoteState::kConfirmed;
    SaveWeaponSelectSnap(slot.ws_snap);
    slot.ws_snap.ws_done = kWsDone;
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
  uint32_t const kInputFrame = simFrame_ + inputDelay_;
  if (!lastSentFrameValid_ || kInputFrame != lastSentFrame_) {
    uint32_t const kSlot = kInputFrame % kInputBufferSize;
    // Mask to the 7 ControlState bits (Up/Down/Left/Right/Fire/Change/
    // Jump); bit 7 is reserved and must not leak onto the wire.
    localInputs_[kSlot] = localControlState_.Pack() & 0x7f;
    lastSentFrame_ = kInputFrame;
    lastSentFrameValid_ = true;
  }

  // Emit the last K = kMaxRollback + 1 local inputs as a redundant
  // batch every tick. The redundancy covers single dropped packets
  // without a retransmit RTT. Send continues even when stalled below so
  // the remote peer can promote out of its own stall.
  SendInputWindow(kInputFrame, simFrame_);

  // Walk confirmedSimFrame_+1 .. simFrame-1 in order: promote slots whose
  // prediction matched, stop at the first mismatch and let the resim
  // below reload its predecessor's snapshot.
  int32_t rollback_to = -1;
  while (confirmedSimFrame_ + 1 < static_cast<int32_t>(simFrame_)) {
    int32_t const kF = confirmedSimFrame_ + 1;
    uint32_t const kS = static_cast<uint32_t>(kF) % kInputBufferSize;
    if (!remoteInputReady_[kS]) {
      break;
    }
    uint8_t const kReal = remoteInputs_[kS];

    auto* slot = rollbackBuffer_.Find(kF);
    bool match = true;
    bool const kWasPredicted = slot && slot->remote_state == rollback::RemoteState::kPredicted;
    if (kWasPredicted) {
      // slot stores inputs by worm index: localInput=worm0, remoteInput=worm1.
      // The misprediction question is about the *other* peer's input.
      uint8_t const kStoredOther = (localIdx_ == 0) ? slot->remote_input : slot->local_input;
      match = (kStoredOther == kReal);
    }

    if (!match) {
      rollback_to = kF - 1;
      break;
    }

    if (slot) {
      slot->remote_state = rollback::RemoteState::kConfirmed;
    }
    lastRemoteInput_ = kReal;
    remoteInputReady_[kS] = false;
    confirmedSimFrame_ = kF;

    // The forward path skipped sending a checksum when the frame was
    // first run as predicted; emit the cached value now that we've
    // confirmed it.
    if (kWasPredicted && sendChecksum_) {
      sendChecksum_(generation_, static_cast<uint32_t>(kF), slot->checksum);
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

    uint8_t const kLastGoodWorm0 = last_good->local_input;
    uint8_t const kLastGoodWorm1 = last_good->remote_input;
    localPrevInput_ = (localIdx_ == 0) ? kLastGoodWorm0 : kLastGoodWorm1;
    remotePrevInput_ = (localIdx_ == 0) ? kLastGoodWorm1 : kLastGoodWorm0;

    game.SetSpeculative(/*s=*/true);
    for (int32_t f = rollback_to + 1; std::cmp_less(f, simFrame_); ++f) {
      uint32_t const kS = static_cast<uint32_t>(f) % kInputBufferSize;
      uint8_t const kCurLocal = localInputs_[kS];
      uint8_t cur_remote = 0;
      bool frame_predicted = false;
      // Same contiguity rule as the new-frame block — only consume real
      // input (and clear the ring entry) when the chain reaches F-1.
      // Out-of-order real inputs stay in the ring for a later promote.
      bool const kResimContiguous = (confirmedSimFrame_ + 1 == f);
      if (remoteInputReady_[kS] && kResimContiguous) {
        cur_remote = remoteInputs_[kS];
        remoteInputReady_[kS] = false;
        lastRemoteInput_ = cur_remote;
        frame_predicted = false;
        confirmedSimFrame_ = f;
      } else {
        cur_remote = lastRemoteInput_;
        frame_predicted = true;
      }

      uint8_t const kRisingLocal = kCurLocal & ~localPrevInput_;
      uint8_t const kRisingRemote = cur_remote & ~remotePrevInput_;
      uint8_t const kReleasedLocal = localPrevInput_ & ~kCurLocal;
      uint8_t const kReleasedRemote = remotePrevInput_ & ~cur_remote;
      game.worms[localIdx_]->control_states.istate |= kRisingLocal;
      game.worms[remoteIdx_]->control_states.istate |= kRisingRemote;
      game.worms[localIdx_]->control_states.istate &= ~kReleasedLocal;
      game.worms[remoteIdx_]->control_states.istate &= ~kReleasedRemote;
      localPrevInput_ = kCurLocal;
      remotePrevInput_ = cur_remote;

      game.ProcessFrame();

      auto& out_slot = rollbackBuffer_.Write(static_cast<int>(f));
      out_slot.local_input = (localIdx_ == 0) ? kCurLocal : cur_remote;
      out_slot.remote_input = (localIdx_ == 0) ? cur_remote : kCurLocal;
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
    game.SetSpeculative(/*s=*/false);
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

  uint32_t const kCurrentSlot = simFrame_ % kInputBufferSize;

  uint8_t const kCurLocal = localInputs_[kCurrentSlot];
  uint8_t cur_remote = 0;
  bool predicted = false;
  // Only consume real remote input when the confirmed chain reaches
  // simFrame-1. Out-of-order arrival (real input for a future frame
  // while an earlier one is still missing) must stay in the ring so a
  // later promote can fold it into a contiguous chain.
  bool const kChainContiguous = confirmedSimFrame_ + 1 == static_cast<int32_t>(simFrame_);
  if (remoteInputReady_[kCurrentSlot] && kChainContiguous) {
    cur_remote = remoteInputs_[kCurrentSlot];
    remoteInputReady_[kCurrentSlot] = false;
    lastRemoteInput_ = cur_remote;
    predicted = false;
  } else {
    cur_remote = lastRemoteInput_;
    predicted = true;
  }

  uint8_t const kRisingLocal = kCurLocal & ~localPrevInput_;
  uint8_t const kRisingRemote = cur_remote & ~remotePrevInput_;
  uint8_t const kReleasedLocal = localPrevInput_ & ~kCurLocal;
  uint8_t const kReleasedRemote = remotePrevInput_ & ~cur_remote;

  game.worms[localIdx_]->control_states.istate |= kRisingLocal;
  game.worms[remoteIdx_]->control_states.istate |= kRisingRemote;
  game.worms[localIdx_]->control_states.istate &= ~kReleasedLocal;
  game.worms[remoteIdx_]->control_states.istate &= ~kReleasedRemote;

  localPrevInput_ = kCurLocal;
  remotePrevInput_ = cur_remote;

  // First execution of this frame, predicted or not — run it audible.
  // Under real latency remote input is almost never in by now, so
  // gating sound on `predicted` would mute the whole match (a frame
  // whose prediction holds is later promoted without re-execution, so
  // its sounds would never be heard at all). Only resim re-executions
  // are speculative; a misprediction can play a sound that "didn't
  // happen", which is the standard rollback trade-off.
  game.ProcessFrame();
  ++simFrame_;

  if (!predicted) {
    confirmedSimFrame_ = static_cast<int32_t>(simFrame_) - 1;
  }

  // Snapshot the post-frame state and cache the checksum (used by a
  // later promote/resim if the frame was predicted).
  {
    int const kSnapFrame = static_cast<int>(simFrame_) - 1;
    rollback::Slot& slot = rollbackBuffer_.Write(kSnapFrame);
    slot.local_input = (localIdx_ == 0) ? kCurLocal : cur_remote;
    slot.remote_input = (localIdx_ == 0) ? cur_remote : kCurLocal;
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
    if (!goingToMenu_) {
      EnterGoingToMenu(180);
    }
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
    // NOLINTNEXTLINE(cert-err33-c) — buffer is generous for a small unsigned.
    std::snprintf(buf, sizeof(buf), "RB:%u", lastTickResimFrames_);
    font.DrawString(renderer.bmp, buf, 2, renderer.render_res_y - 9, 50);
  }

  if (IsPaused()) {
    Fill(renderer.bmp, 0);
    Common& common = *game.common;
    Font& font = common.font;
    int const kCx = renderer.render_res_x / 2;
    int const kCy = renderer.render_res_y / 2 - 20;

    renderer.pal = game.common->exepal;
    renderer.pal.RotateFrom(game.common->exepal, 168, 174, gfx.menu_cycles);
    renderer.pal.Fade(fadeValue_);

    if (localPaused_) {
      std::string const kTitle = "GAME PAUSED";
      int const kTw = font.GetDims(kTitle);
      font.DrawString(renderer.bmp, kTitle, kCx - kTw / 2, kCy, 50);

      pauseMenu_.Place(kCx, kCy + 16);
      pauseMenu_.Draw(common, renderer, /*disabled=*/false);
    } else {
      std::string const kTitle = "PAUSED BY PEER";
      int const kTw = font.GetDims(kTitle);
      font.DrawString(renderer.bmp, kTitle, kCx - kTw / 2, kCy, 50);

      std::string const kHint = "PRESS ESC TO DISCONNECT";
      int const kHw = font.GetDims(kHint);
      font.DrawString(renderer.bmp, kHint, kCx - kHw / 2, kCy + 16, 6);
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
