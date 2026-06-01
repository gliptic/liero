#include "rollbackController.hpp"

#include "../gfx.hpp"
#include "../mixer/player.hpp"
#include "../replay.hpp"
#include "../viewport.hpp"
#include "../spectatorviewport.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <miniz.h>

// Shared two-player setup for the live and shadow games. Both must
// produce identical processFrame paths, so the worm slots and main
// viewports are configured here from a single source of truth. The
// live ctor also adds a SpectatorViewport; the shadow does not need
// one.
static void configureGameSlots(
    Game& g, std::array<std::shared_ptr<WormSettings>, 2> ws) {
  for (int idx = 0; idx < 2; ++idx) {
    auto worm = std::make_shared<Worm>();
    worm->settings = ws[idx];
    worm->health = worm->settings->health;
    worm->index = idx;
    worm->statsX = idx == 0 ? 0 : 218;
    g.addWorm(worm);
  }
  g.addViewport(new Viewport(Rect(0, 0, 158, 158), 0, 504, 350));
  g.addViewport(new Viewport(Rect(160, 0, 158 + 160, 158), 1, 504, 350));
}

RollbackController::RollbackController(
    std::shared_ptr<Common> common,
    std::shared_ptr<Settings> settings,
    int localPlayerIdx)
    : game(common, settings, gfx.soundPlayer)
    , localIdx(localPlayerIdx)
    , remoteIdx(localPlayerIdx ^ 1)
    , state(StateInitial)
    , fadeValue(0)
    , goingToMenu(false)
    , simFrame(0)
    , inputDelay(3)
    , lastSentFrame(0)
    , lastSentFrameValid(false)
    , rollbackBufferPrepared_(false)
    , confirmedSimFrame_(-1)
    , lastRemoteInput_(0)
{
  localPrevInput = 0;
  remotePrevInput = 0;
  localHeldFrames.fill(0);
  remoteHeldFrames.fill(0);
  skipWeaponSelection = false;
  levelPreloaded = false;
  localPaused_ = false;
  remotePaused_ = false;

  pauseMenu_.init(true);
  pauseMenu_.addItem(MenuItem(7, 6, "RESUME", 0));
  pauseMenu_.addItem(MenuItem(7, 6, "END MATCH", 2));
  pauseMenu_.addItem(MenuItem(7, 6, "DISCONNECT", 1));

  localInputs.fill(0);
  remoteInputs.fill(0);
  remoteInputReady.fill(false);

  std::array<std::shared_ptr<WormSettings>, 2> ws;
  for (int idx = 0; idx < 2; ++idx) {
    ws[idx] = (idx == localIdx)
        ? settings->wormSettings[Settings::NetworkPlayerIdx]
        : settings->wormSettings[idx];
  }
  configureGameSlots(game, ws);
  game.addSpectatorViewport(
      new SpectatorViewport(Rect(0, 0, 504 + 68, 350), 504, 350));
}

RollbackController::~RollbackController() {}

void RollbackController::loadLevelFromData(const std::vector<uint8_t>& data) {
  if (data.size() < 5)
    return;

  bool isCompressed = (data[0] != 0);
  uint32_t rawSize;
  std::memcpy(&rawSize, data.data() + 1, 4);

  static constexpr uint32_t MAX_RAW_SIZE = 10 * 1024 * 1024;
  if (rawSize > MAX_RAW_SIZE)
    return;

  std::vector<uint8_t> raw;
  if (isCompressed) {
    raw.resize(rawSize);
    mz_ulong destLen = rawSize;
    int status = mz_uncompress(raw.data(), &destLen, data.data() + 5,
                               static_cast<mz_ulong>(data.size() - 5));
    if (status != MZ_OK)
      return;
  } else {
    raw.assign(data.begin() + 5, data.end());
  }

  if (raw.size() < 8)
    return;

  uint16_t w, h;
  std::memcpy(&w, raw.data(), 2);
  std::memcpy(&h, raw.data() + 2, 2);

  if (w == 0 || h == 0 || w > 4096 || h > 4096)
    return;

  uint32_t randStateLen;
  std::memcpy(&randStateLen, raw.data() + 4, 4);

  if (randStateLen > 64 * 1024)
    return;
  if (raw.size() < 8 + randStateLen + 4)
    return;

  std::string randState(reinterpret_cast<const char*>(raw.data() + 8),
                        randStateLen);
  uint32_t randLast;
  std::memcpy(&randLast, raw.data() + 8 + randStateLen, 4);

  size_t pixelsOffset = 8 + randStateLen + 4;
  size_t pixelDataSize = static_cast<size_t>(w) * h;
  if (raw.size() < pixelsOffset + pixelDataSize + 768)
    return;

  game.level.resize(w, h);
  Common& common = *game.common;

  const uint8_t* pixels = raw.data() + pixelsOffset;
  for (size_t i = 0; i < pixelDataSize; ++i) {
    game.level.data[i] = pixels[i];
    game.level.materials[i] = common.materials[pixels[i]];
  }

  const uint8_t* palData = raw.data() + pixelsOffset + pixelDataSize;
  for (int i = 0; i < 256; ++i) {
    game.level.origpal.entries[i].r = palData[i * 3 + 0];
    game.level.origpal.entries[i].g = palData[i * 3 + 1];
    game.level.origpal.entries[i].b = palData[i * 3 + 2];
  }

  game.rand.deserialize(randState);
  game.rand.last = randLast;

  levelPreloaded = true;
}

void RollbackController::setInputCallbacks(InputBatchSendCallback send) {
  sendInputBatch = std::move(send);
}

void RollbackController::sendInputWindow(uint32_t newestFrame,
                                         uint32_t localFrame) {
  if (!sendInputBatch) return;
  constexpr uint8_t K = static_cast<uint8_t>(rollback::kMaxRollback + 1);
  uint8_t count;
  uint32_t baseFrame;
  if (newestFrame + 1u < K) {
    count = static_cast<uint8_t>(newestFrame + 1u);
    baseFrame = 0;
  } else {
    count = K;
    baseFrame = newestFrame - (K - 1u);
  }
  std::array<uint8_t, K> window{};
  for (uint8_t i = 0; i < count; ++i) {
    window[i] = localInputs[(baseFrame + i) % INPUT_BUFFER_SIZE];
  }
  sendInputBatch(generation_, baseFrame, count, window.data(), localFrame);
}

void RollbackController::injectRemoteBatch(uint8_t generation,
                                           uint32_t baseFrame, uint8_t count,
                                           uint8_t const* inputs,
                                           uint32_t remoteLocalFrame) {
  // Same-generation packets feed the input ring; gen+1 packets are
  // buffered until our own phase transition fires (replayed in
  // resetForGamePhase). Older or further-future packets are unrecoverable.
  if (generation == generation_) {
    for (uint8_t i = 0; i < count; ++i) {
      injectRemoteInput(baseFrame + i, inputs[i]);
    }
    // Monotonic — an out-of-order stale packet must not pull our
    // knowledge of the remote's progress backwards.
    int32_t f = static_cast<int32_t>(remoteLocalFrame);
    if (f > lastKnownRemoteFrame_) lastKnownRemoteFrame_ = f;
    return;
  }

  if (generation == static_cast<uint8_t>(generation_ + 1) &&
      count <= kMaxPendingFutureBatches) {
    if (pendingFutureCount_ < kMaxPendingFutureBatches) {
      auto& slot = pendingFutureBatches_[pendingFutureCount_++];
      slot.baseFrame = baseFrame;
      slot.count = count;
      for (uint8_t i = 0; i < count; ++i) slot.inputs[i] = inputs[i];
      slot.remoteLocalFrame = remoteLocalFrame;
      return;
    }
  }

  ++droppedOldGenerationBatches_;
}

void RollbackController::injectRemoteInput(uint32_t frame, uint8_t input) {
  // Redundant batch packets routinely overlap the confirmation boundary;
  // re-injecting already-confirmed frames would re-set remoteInputReady
  // on a ring slot that wraps into the live rollback window.
  if (static_cast<int32_t>(frame) <= confirmedSimFrame_) return;
  uint32_t slot = frame % INPUT_BUFFER_SIZE;
  remoteInputs[slot] = input;
  remoteInputReady[slot] = true;
}

void RollbackController::onKey(int key, bool keyState) {
  Worm::Control control;
  Worm* worm = game.worms[localIdx].get();
  bool found = false;

  if (worm->settings->inputDevice == WormSettingsExtensions::InputKeyboard) {
    uint32_t* controls = game.settings->extensions
        ? worm->settings->controlsEx : worm->settings->controls;
    std::size_t maxControl = game.settings->extensions
        ? WormSettings::MaxControlEx : WormSettings::MaxControl;
    for (std::size_t c = 0; c < maxControl; ++c) {
      if (controls[c] == static_cast<uint32_t>(key)) {
        control = static_cast<Worm::Control>(c);
        found = true;
        break;
      }
    }
  }

  if (found) {
    worm->cleanControlStates.set(control, keyState);

    if (control < Worm::MaxControl) {
      localControlState.set(control, keyState);
    }

    if (worm->cleanControlStates[WormSettings::Dig]) {
      localControlState.set(Worm::Left, true);
      localControlState.set(Worm::Right, true);
    } else {
      if (!worm->cleanControlStates[Worm::Left])
        localControlState.set(Worm::Left, false);
      if (!worm->cleanControlStates[Worm::Right])
        localControlState.set(Worm::Right, false);
    }
  }

  if (key == DkEscape && keyState) {
    if (localPaused_) {
      localPaused_ = false;
      if (onLocalResume) onLocalResume();
    } else if (remotePaused_ && !goingToMenu) {
      // Remote is paused, local Escapes — treat as a disconnect so the
      // peer learns and tears down in lockstep instead of waiting for
      // socket timeout.
      if (onPeerLeft) onPeerLeft();
      peerLeft();
    } else if (!goingToMenu) {
      localPaused_ = true;
      pauseMenu_.moveToFirstVisible();
      if (onLocalPause) onLocalPause();
    }
  }
}

void RollbackController::unfocus() {
  if (state == StateWeaponSelection && ws)
    ws->unfocus();
  localPaused_ = false;
  remotePaused_ = false;
}

void RollbackController::focus() {
  if (state == StateGameEnded) {
    goingToMenu = true;
    fadeValue = 0;
    return;
  }
  if (state == StateWeaponSelection)
    ws->focus();
  if (state == StateInitial) {
    if (!levelPreloaded)
      game.level.generateFromSettings(*game.common, *game.settings, game.rand);

    if (skipWeaponSelection) {
      for (auto const& w : game.worms)
        w->initWeapons(game);
      for (auto const& w : game.worms)
        w->lives = game.settings->lives;
      game.startGame();
      game.resetWorms();
      state = StateGame;

      seedRollbackAndShadow();
    } else {
      state = StateWeaponSelection;

      for (auto const& w : game.worms) {
        w->settings->controller = 0;
      }

      ws = std::make_unique<WeaponSelection>(game);
    }
  }
  game.focus(gfx.playRenderer);
  game.focus(gfx.singleScreenRenderer);
  goingToMenu = false;
  fadeValue = 0;

  // Size the rollback ring buffer once the level (and therefore the
  // GameSnapshot vector sizes) are known.
  if (!rollbackBufferPrepared_) {
    rollbackBuffer_.prepare(game);
    rollbackBufferPrepared_ = true;
  }
}

bool RollbackController::process() {
  if (isPaused()) {
    if (fadeValue < 33)
      fadeValue += 1;

    if (localPaused_) {
      if (gfx.testSDLKeyOnce(SDL_SCANCODE_UP)
       || gfx.testControlOnce(WormSettingsExtensions::Up)
       || gfx.testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        g_soundPlayer->play(game.common->soundHook[SoundMenuMoveDown]);
        pauseMenu_.movement(-1);
      }

      if (gfx.testSDLKeyOnce(SDL_SCANCODE_DOWN)
       || gfx.testControlOnce(WormSettingsExtensions::Down)
       || gfx.testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        g_soundPlayer->play(game.common->soundHook[SoundMenuMoveUp]);
        pauseMenu_.movement(1);
      }

      if (gfx.testSDLKeyOnce(SDL_SCANCODE_RETURN)
       || gfx.testSDLKeyOnce(SDL_SCANCODE_KP_ENTER)
       || gfx.testControlOnce(WormSettingsExtensions::Fire)
       || gfx.testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
        int sel = pauseMenu_.selectedId();
        if (sel == 0) {
          localPaused_ = false;
          if (onLocalResume) onLocalResume();
        } else if (sel == 2) {
          localPaused_ = false;
          if (onLocalResume) onLocalResume();
          if (onEndMatch) onEndMatch();
          endMatch();
        } else {
          if (onLocalResume) onLocalResume();
          if (onPeerLeft) onPeerLeft();
          peerLeft();
        }
      }
    }

    return true;
  }

  lastTickResimFrames_ = 0;

  if (state == StateWeaponSelection) {
    advanceWeaponSelection();
  } else if (state == StateGame || state == StateGameEnded) {
    advanceSimulation();
  }

  if (goingToMenu) {
    if (fadeValue > 0)
      fadeValue -= 1;
    else {
      if (state == StateGameEnded) {
        // Stats live on the shadow (see setupShadowGame); finalize there
        // so gameTime / lifeSpans reflect the confirmed timeline.
        Game* sg = statsGame();
        sg->statsRecorder->finish(*sg);
      }
      return false;
    }
  } else {
    if (fadeValue < 33)
      fadeValue += 1;
  }

  return true;
}

bool RollbackController::weaponSelectStep(uint8_t curLocal, uint8_t curRemote) {
  uint8_t risingLocal = curLocal & ~localPrevInput;
  uint8_t risingRemote = curRemote & ~remotePrevInput;
  uint8_t releasedLocal = localPrevInput & ~curLocal;
  uint8_t releasedRemote = remotePrevInput & ~curRemote;

  game.worms[localIdx]->controlStates.istate |= risingLocal;
  game.worms[remoteIdx]->controlStates.istate |= risingRemote;
  game.worms[localIdx]->controlStates.istate &= ~releasedLocal;
  game.worms[remoteIdx]->controlStates.istate &= ~releasedRemote;

  for (int bit = 0; bit < 7; ++bit) {
    uint8_t mask = 1 << bit;
    if (risingLocal & mask) {
      localHeldFrames[bit] = 0;
    } else if (curLocal & mask) {
      ++localHeldFrames[bit];
      if (localHeldFrames[bit] >= KEY_REPEAT_INITIAL &&
          (localHeldFrames[bit] - KEY_REPEAT_INITIAL) % KEY_REPEAT_INTERVAL == 0) {
        game.worms[localIdx]->controlStates.istate |= mask;
      }
    } else {
      localHeldFrames[bit] = 0;
    }
    if (risingRemote & mask) {
      remoteHeldFrames[bit] = 0;
    } else if (curRemote & mask) {
      ++remoteHeldFrames[bit];
      if (remoteHeldFrames[bit] >= KEY_REPEAT_INITIAL &&
          (remoteHeldFrames[bit] - KEY_REPEAT_INITIAL) % KEY_REPEAT_INTERVAL == 0) {
        game.worms[remoteIdx]->controlStates.istate |= mask;
      }
    } else {
      remoteHeldFrames[bit] = 0;
    }
  }

  localPrevInput = curLocal;
  remotePrevInput = curRemote;

  return ws->processFrame();
}

void RollbackController::saveWeaponSelectSnap(WeaponSelectSnap& snap) const {
  snap.valid = true;
  for (int i = 0; i < 2; ++i) {
    Worm const& w = *game.worms[i];
    WormSettings const& wsCfg = *w.settings;
    auto& p = snap.players[i];
    for (int j = 0; j < Settings::selectableWeapons; ++j) {
      p.weapons[j] = wsCfg.weapons[j];
    }
    p.isReady = ws->isReady[i];
    p.menuSelection = ws->menus[i].selection();
    p.menuTopItem = ws->menus[i].topItem;
    p.menuBottomItem = ws->menus[i].bottomItem;
    p.wormControlStates = static_cast<uint16_t>(w.controlStates.istate);
    p.currentWeapon = w.currentWeapon;
  }
  snap.rand = game.rand;
  snap.localPrevInput = localPrevInput;
  snap.remotePrevInput = remotePrevInput;
  snap.localHeldFrames = localHeldFrames;
  snap.remoteHeldFrames = remoteHeldFrames;
}

void RollbackController::loadWeaponSelectSnap(WeaponSelectSnap const& snap) {
  Common const& common = *game.common;
  for (int i = 0; i < 2; ++i) {
    Worm& w = *game.worms[i];
    WormSettings& wsCfg = *w.settings;
    auto const& p = snap.players[i];
    for (int j = 0; j < Settings::selectableWeapons; ++j) {
      wsCfg.weapons[j] = p.weapons[j];
      int weapOrderIdx = static_cast<int>(p.weapons[j]) - 1;
      if (weapOrderIdx >= 0 &&
          weapOrderIdx < static_cast<int>(common.weapOrder.size())) {
        int w_idx = common.weapOrder[weapOrderIdx];
        w.weapons[j].type = &common.weapons[w_idx];
        // menus[i].items index 0 is "Randomize", indices [1..N] are the
        // weapon slots, index N+1 is "Done".
        if (j + 1 < static_cast<int>(ws->menus[i].items.size())) {
          ws->menus[i].items[j + 1].string = common.weapons[w_idx].name;
        }
      }
    }
    ws->isReady[i] = p.isReady;
    ws->menus[i].setSelection(p.menuSelection);
    ws->menus[i].topItem = p.menuTopItem;
    ws->menus[i].bottomItem = p.menuBottomItem;
    w.controlStates.istate = p.wormControlStates;
    w.currentWeapon = p.currentWeapon;
  }
  game.rand = snap.rand;
  localPrevInput = snap.localPrevInput;
  remotePrevInput = snap.remotePrevInput;
  localHeldFrames = snap.localHeldFrames;
  remoteHeldFrames = snap.remoteHeldFrames;
}

void RollbackController::resetForGamePhase() {
  localInputs.fill(0);
  remoteInputs.fill(0);
  remoteInputReady.fill(false);

  simFrame = 0;
  confirmedSimFrame_ = -1;
  lastSentFrame = 0;
  lastSentFrameValid = false;
  lastRemoteInput_ = 0;
  lastKnownRemoteFrame_ = -1;

  // Edge-detection state — carrying these across the phase boundary
  // would produce a spurious rising/released edge on the first frame.
  localPrevInput = 0;
  remotePrevInput = 0;
  localHeldFrames.fill(0);
  remoteHeldFrames.fill(0);

  rollbackBuffer_.clear();
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
    injectRemoteBatch(generation_, s.baseFrame, s.count, s.inputs.data(),
                      s.remoteLocalFrame);
  }
}

void RollbackController::finishWeaponSelect() {
  if (state != StateWeaponSelection) return;

  ws->finalize();
  ws.reset();

  for (auto const& w : game.worms) {
    w->lives = game.settings->lives;
  }
  game.startGame();
  game.resetWorms();
  state = StateGame;

  // The WS phase can leave peers at different simFrame counters
  // (asymmetric stalls + WS-rollback resims). Carrying that skew into
  // game phase would silently diverge every simFrame-keyed comparison
  // downstream (checksums, terrain destruction) and trip the desync
  // detector. Resetting here gives the game phase a symmetric baseline.
  resetForGamePhase();

  seedRollbackAndShadow();
}

void RollbackController::seedRollbackAndShadow() {
  // Game state vectors are fully sized only after startGame; prepare
  // here so the slot-0 snapshot below captures the right widths.
  if (!rollbackBufferPrepared_) {
    rollbackBuffer_.prepare(game);
    rollbackBufferPrepared_ = true;
  }

  // Seed slot[0] = post-startGame state so a misprediction on the first
  // game-phase frame has a valid rollback target, and so setupShadowGame
  // has a frame-0 snapshot to clone into the shadow. The first
  // process() tick will overwrite this with the post-frame-0 snapshot.
  rollback::Slot& seed = rollbackBuffer_.write(0);
  seed.localInput = 0;
  seed.remoteInput = 0;
  seed.remoteState = rollback::RemoteState::Confirmed;
  seed.wsSnap.valid = false;
  game.saveSnapshotFast(seed.snapshot);
  seed.checksum = wideRollbackChecksum(game);

  setupShadowGame();
}

void RollbackController::setupShadowGame() {
  // Mirror the live game's construction: same Common/Settings, silent
  // sound player, identical worm+viewport configuration so the
  // snapshot we load below produces an identical processFrame path.
  shadowGame_ = std::make_unique<Game>(
      game.common, game.settings, std::make_shared<NullSoundPlayer>());

  configureGameSlots(
      *shadowGame_,
      {game.worms[0]->settings, game.worms[1]->settings});

  // loadSnapshotFast assumes level buffers are already sized; the
  // snapshot itself carries pixel data but not dimensions.
  shadowGame_->level.width = game.level.width;
  shadowGame_->level.height = game.level.height;
  std::size_t cells = static_cast<std::size_t>(game.level.width)
                    * static_cast<std::size_t>(game.level.height);
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
  for (auto const& w : shadowGame_->worms) w->initWeapons(*shadowGame_);
  shadowGame_->paused = false;
  shadowGame_->startGame();
  shadowGame_->resetWorms();

  // Frame-0 seed: live game's post-startGame state.
  if (auto* seedSlot = rollbackBuffer_.find(0)) {
    shadowGame_->loadSnapshotFast(seedSlot->snapshot);
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
  game.statsRecorder.reset(new StatsRecorder);

  startReplayRecording();
}

void RollbackController::startReplayRecording() {
  if (!shadowGame_) return;

  // Test path: caller provided a writer directly; skip the gfx-driven
  // file-naming logic and just hand it to ReplayWriter.
  if (replayWriterOverride_) {
    try {
      shadowReplay_ = std::make_unique<ReplayWriter>(
          std::move(replayWriterOverride_));
      shadowReplay_->beginRecord(*shadowGame_);
    } catch (std::runtime_error& e) {
      std::fprintf(stderr, "[replay] failed to start recording: %s\n", e.what());
      shadowReplay_.reset();
    }
    return;
  }

  if (!Settings::extensions || !game.settings->recordReplays) return;

  // Tests construct RollbackControllers without first wiring up gfx,
  // so getUserConfigNode() returns a default-constructed FsNode with
  // a null impl pointer. Operator/ would dereference that and segv.
  FsNode configRoot = gfx.getUserConfigNode();
  if (!configRoot.imp) return;

  try {
    std::time_t ticks = std::time(nullptr);
    std::tm* now = std::localtime(&ticks);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H.%M.%S", now);

    std::string playerNames = " ";
    for (std::size_t i = 0; i < shadowGame_->worms.size() && i < 2; ++i) {
      Worm& worm = *shadowGame_->worms[i];
      std::string const& name = worm.settings->name;
      if (i > 0) playerNames.push_back('-');
      int chars = 0;
      for (std::size_t c = 0; c < name.size() && chars < 4; ++c, ++chars) {
        unsigned char ch = static_cast<unsigned char>(name[c]);
        if (std::isalnum(ch)) playerNames.push_back(static_cast<char>(ch));
      }
    }

    // The " mp{idx}" suffix distinguishes the two peers' recordings if
    // they share a config directory and prevents the host's and
    // client's files from colliding.
    char suffix[8];
    std::snprintf(suffix, sizeof(suffix), " mp%d", localIdx);

    auto node = configRoot / "Replays"
                  / (std::string(timeBuf) + playerNames + suffix + ".lrp");

    shadowReplay_ = std::make_unique<ReplayWriter>(node.toWriter());
    shadowReplay_->beginRecord(*shadowGame_);
  } catch (std::runtime_error& e) {
    std::fprintf(stderr, "[replay] failed to start recording: %s\n", e.what());
    shadowReplay_.reset();
  }
}

void RollbackController::stopReplayRecording() {
  // ReplayWriter destructor writes the 0x83 terminator.
  shadowReplay_.reset();
}

void RollbackController::driveShadow() {
  if (!shadowGame_) return;

  while (shadowFrame_ < confirmedSimFrame_) {
    int32_t F = shadowFrame_ + 1;
    rollback::Slot const* slot = rollbackBuffer_.find(F);
    if (!slot) {
      // The ring only holds kMaxRollback+1 slots; if the shadow ever
      // falls more than that behind, we can't reconstruct the missing
      // frame's inputs and have to give up on this match's recording.
      shadowGame_.reset();
      return;
    }

    uint8_t curLocal  = (localIdx == 0) ? slot->localInput : slot->remoteInput;
    uint8_t curRemote = (localIdx == 0) ? slot->remoteInput : slot->localInput;

    uint8_t risingLocal  = curLocal  & ~shadowLocalPrevInput_;
    uint8_t risingRemote = curRemote & ~shadowRemotePrevInput_;
    uint8_t releasedLocal  = shadowLocalPrevInput_  & ~curLocal;
    uint8_t releasedRemote = shadowRemotePrevInput_ & ~curRemote;
    shadowGame_->worms[localIdx ]->controlStates.istate |= risingLocal;
    shadowGame_->worms[remoteIdx]->controlStates.istate |= risingRemote;
    shadowGame_->worms[localIdx ]->controlStates.istate &= ~releasedLocal;
    shadowGame_->worms[remoteIdx]->controlStates.istate &= ~releasedRemote;
    shadowLocalPrevInput_  = curLocal;
    shadowRemotePrevInput_ = curRemote;

    // recordFrame() reads worm.controlStates ^ prevControlStates, so it
    // must run after edge detection but before processFrame() (which
    // sets prev = current at end of tick, wiping the delta).
    if (shadowReplay_) {
      try {
        shadowReplay_->recordFrame();
      } catch (std::runtime_error& e) {
        std::fprintf(stderr, "[replay] aborting recording at frame %d: %s\n",
                     F, e.what());
        shadowReplay_.reset();
      }
    }

    shadowGame_->processFrame();
    shadowFrame_ = F;

    // Sanity check: the shadow must match the live game's confirmed
    // state for the same frame. Log once on divergence so the breakage
    // surfaces without spamming stderr.
    uint32_t shadowChk = wideRollbackChecksum(*shadowGame_);
    if (shadowChk != slot->checksum && !shadowMismatchLogged_) {
      std::fprintf(stderr,
          "[replay shadow] mismatch at frame %d: shadow=%08x live=%08x"
          " curLocal=%02x curRemote=%02x slot.localInput=%02x slot.remoteInput=%02x"
          " shadowRand=%08x\n",
          F, shadowChk, slot->checksum, curLocal, curRemote,
          slot->localInput, slot->remoteInput, shadowGame_->rand.last);
      shadowMismatchLogged_ = true;
    }
  }
}

// Mirror of advanceSimulation() for the weapon-select phase: same
// promote-confirmed-prefix → rollback-to-mismatch → resim-speculative →
// save-snapshot shape, but stepping weaponSelectStep instead of
// processFrame and saving into wsSnap instead of snapshot. Keep the two
// in sync — a fix here likely needs to land in the sibling too.
void RollbackController::advanceWeaponSelection() {
  uint32_t inputFrame = simFrame + inputDelay;
  if (!lastSentFrameValid || inputFrame != lastSentFrame) {
    uint32_t slot = inputFrame % INPUT_BUFFER_SIZE;
    // Mask to the 7 ControlState bits (Up/Down/Left/Right/Fire/Change/
    // Jump); bit 7 is reserved and must not leak onto the wire.
    localInputs[slot] = localControlState.pack() & 0x7f;
    lastSentFrame = inputFrame;
    lastSentFrameValid = true;
  }
  sendInputWindow(inputFrame, simFrame);

  // Promote previously-predicted WS frames whose real remote input has
  // now arrived and matches the prediction.
  int32_t rollbackTo = -1;
  while (confirmedSimFrame_ + 1 < static_cast<int32_t>(simFrame)) {
    int32_t F = confirmedSimFrame_ + 1;
    uint32_t s = static_cast<uint32_t>(F) % INPUT_BUFFER_SIZE;
    if (!remoteInputReady[s]) break;
    uint8_t real = remoteInputs[s];

    auto* slot = rollbackBuffer_.find(F);
    bool match = true;
    bool wasPredicted =
        slot && slot->remoteState == rollback::RemoteState::Predicted;
    if (wasPredicted) {
      uint8_t storedOther =
          (localIdx == 0) ? slot->remoteInput : slot->localInput;
      match = (storedOther == real);
    }

    if (!match) {
      rollbackTo = F - 1;
      break;
    }

    if (slot) slot->remoteState = rollback::RemoteState::Confirmed;
    lastRemoteInput_ = real;
    remoteInputReady[s] = false;
    confirmedSimFrame_ = F;
  }

  // Rollback resim for the weapon-select phase. Identical structure to
  // advanceSimulation's resim, but with weaponSelectStep / wsSnap in
  // place of processFrame / GameSnapshot.
  if (rollbackTo >= 0) {
    ++rollbackCount_;
    lastTickResimFrames_ +=
        static_cast<uint32_t>(static_cast<int32_t>(simFrame) - rollbackTo - 1);
    auto* lastGood = rollbackBuffer_.find(rollbackTo);
    if (lastGood && lastGood->wsSnap.valid) {
      loadWeaponSelectSnap(lastGood->wsSnap);
    }

    uint8_t lastGoodWorm0 = lastGood ? lastGood->localInput : 0;
    uint8_t lastGoodWorm1 = lastGood ? lastGood->remoteInput : 0;
    localPrevInput  = (localIdx == 0) ? lastGoodWorm0 : lastGoodWorm1;
    remotePrevInput = (localIdx == 0) ? lastGoodWorm1 : lastGoodWorm0;

    game.setSpeculative(true);
    for (int32_t F = rollbackTo + 1; F < static_cast<int32_t>(simFrame); ++F) {
      uint32_t s = static_cast<uint32_t>(F) % INPUT_BUFFER_SIZE;
      uint8_t curLocal = localInputs[s];
      uint8_t curRemote;
      bool framePredicted;
      bool resimContiguous = (confirmedSimFrame_ + 1 == F);
      if (remoteInputReady[s] && resimContiguous) {
        curRemote = remoteInputs[s];
        remoteInputReady[s] = false;
        lastRemoteInput_ = curRemote;
        framePredicted = false;
        confirmedSimFrame_ = F;
      } else {
        curRemote = lastRemoteInput_;
        framePredicted = true;
      }

      bool wsDoneResim = weaponSelectStep(curLocal, curRemote);

      auto& outSlot = rollbackBuffer_.write(static_cast<int>(F));
      outSlot.localInput  = (localIdx == 0) ? curLocal  : curRemote;
      outSlot.remoteInput = (localIdx == 0) ? curRemote : curLocal;
      outSlot.remoteState = framePredicted
          ? rollback::RemoteState::Predicted
          : rollback::RemoteState::Confirmed;
      saveWeaponSelectSnap(outSlot.wsSnap);
      outSlot.wsSnap.wsDone = wsDoneResim;
    }
    game.setSpeculative(false);

    // Resim may have just confirmed the wsDone frame. Re-check here so
    // both peers transition on the same simFrame regardless of whether
    // wsDone was first observed forward or via a resim correcting a
    // mispredicted Fire press.
    if (rollback::Slot const* confSlot =
            rollbackBuffer_.find(confirmedSimFrame_)) {
      if (confSlot->wsSnap.valid && confSlot->wsSnap.wsDone) {
        finishWeaponSelect();
        return;
      }
    }
  }

  // Stall guards — same thresholds as advanceSimulation.
  if (static_cast<int32_t>(simFrame) - confirmedSimFrame_ > rollback::kMaxRollback) {
    return;
  }
  if (lastKnownRemoteFrame_ >= 0 &&
      static_cast<int32_t>(simFrame) - lastKnownRemoteFrame_
          >= frameAdvantageThreshold_) {
    ++frameAdvantageStalls_;
    return;
  }

  // Predict if remote input not yet ready, run ws tick, snapshot.
  uint32_t currentSlot = simFrame % INPUT_BUFFER_SIZE;
  uint8_t curLocal = localInputs[currentSlot];
  uint8_t curRemote;
  bool predicted;
  bool chainContiguous =
      confirmedSimFrame_ + 1 == static_cast<int32_t>(simFrame);
  if (remoteInputReady[currentSlot] && chainContiguous) {
    curRemote = remoteInputs[currentSlot];
    remoteInputReady[currentSlot] = false;
    lastRemoteInput_ = curRemote;
    predicted = false;
  } else {
    curRemote = lastRemoteInput_;
    predicted = true;
  }

  game.setSpeculative(predicted);
  bool wsDone = weaponSelectStep(curLocal, curRemote);
  game.setSpeculative(false);
  ++simFrame;

  if (!predicted) {
    confirmedSimFrame_ = static_cast<int32_t>(simFrame) - 1;
  }

  // Snapshot post-frame weapon-select state.
  {
    int snapFrame = static_cast<int>(simFrame) - 1;
    rollback::Slot& slot = rollbackBuffer_.write(snapFrame);
    slot.localInput = (localIdx == 0) ? curLocal : curRemote;
    slot.remoteInput = (localIdx == 0) ? curRemote : curLocal;
    slot.remoteState = predicted
        ? rollback::RemoteState::Predicted
        : rollback::RemoteState::Confirmed;
    saveWeaponSelectSnap(slot.wsSnap);
    slot.wsSnap.wsDone = wsDone;
  }

  // Transition into game phase as soon as the highest confirmed frame's
  // snapshot shows wsDone=true. Gating on the confirmed slot (rather
  // than the current tick's possibly-predicted wsDone) means a forward
  // confirm, a promote-loop confirm, or a resim-confirm all fire the
  // transition on the same simFrame on both peers. Predicted wsDone
  // snapshots never fire it.
  if (rollback::Slot const* confSlot =
          rollbackBuffer_.find(confirmedSimFrame_)) {
    if (confSlot->wsSnap.valid && confSlot->wsSnap.wsDone) {
      finishWeaponSelect();
    }
  }
}

// Mirror of advanceWeaponSelection() for the game phase. Keep the two
// in sync — a fix here likely needs to land in the sibling too.
void RollbackController::advanceSimulation() {
  uint32_t inputFrame = simFrame + inputDelay;
  if (!lastSentFrameValid || inputFrame != lastSentFrame) {
    uint32_t slot = inputFrame % INPUT_BUFFER_SIZE;
    // Mask to the 7 ControlState bits (Up/Down/Left/Right/Fire/Change/
    // Jump); bit 7 is reserved and must not leak onto the wire.
    localInputs[slot] = localControlState.pack() & 0x7f;
    lastSentFrame = inputFrame;
    lastSentFrameValid = true;
  }

  // Emit the last K = kMaxRollback + 1 local inputs as a redundant
  // batch every tick. The redundancy covers single dropped packets
  // without a retransmit RTT. Send continues even when stalled below so
  // the remote peer can promote out of its own stall.
  sendInputWindow(inputFrame, simFrame);

  // Walk confirmedSimFrame_+1 .. simFrame-1 in order: promote slots whose
  // prediction matched, stop at the first mismatch and let the resim
  // below reload its predecessor's snapshot.
  int32_t rollbackTo = -1;
  while (confirmedSimFrame_ + 1 < static_cast<int32_t>(simFrame)) {
    int32_t F = confirmedSimFrame_ + 1;
    uint32_t s = static_cast<uint32_t>(F) % INPUT_BUFFER_SIZE;
    if (!remoteInputReady[s]) break;
    uint8_t real = remoteInputs[s];

    auto* slot = rollbackBuffer_.find(F);
    bool match = true;
    bool wasPredicted =
        slot && slot->remoteState == rollback::RemoteState::Predicted;
    if (wasPredicted) {
      // slot stores inputs by worm index: localInput=worm0, remoteInput=worm1.
      // The misprediction question is about the *other* peer's input.
      uint8_t storedOther =
          (localIdx == 0) ? slot->remoteInput : slot->localInput;
      match = (storedOther == real);
    }

    if (!match) {
      rollbackTo = F - 1;
      break;
    }

    if (slot) slot->remoteState = rollback::RemoteState::Confirmed;
    lastRemoteInput_ = real;
    remoteInputReady[s] = false;
    confirmedSimFrame_ = F;

    // The forward path skipped sending a checksum when the frame was
    // first run as predicted; emit the cached value now that we've
    // confirmed it.
    if (wasPredicted && sendChecksum) {
      sendChecksum(generation_, static_cast<uint32_t>(F), slot->checksum);
    }
  }

  // Load the last known-good snapshot, then replay every frame after it
  // with the freshest input available. Speculative across the window so
  // previously-emitted sounds/stats don't duplicate.
  if (rollbackTo >= 0) {
    ++rollbackCount_;
    lastTickResimFrames_ +=
        static_cast<uint32_t>(static_cast<int32_t>(simFrame) - rollbackTo - 1);
    auto* lastGood = rollbackBuffer_.find(rollbackTo);
    // Resident by construction: the stall guard caps simFrame - confirmedSimFrame_
    // at kMaxRollback, and the ring holds kMaxRollback+1 slots.
    game.loadSnapshotFast(lastGood->snapshot);

    uint8_t lastGoodWorm0 = lastGood->localInput;
    uint8_t lastGoodWorm1 = lastGood->remoteInput;
    localPrevInput  = (localIdx == 0) ? lastGoodWorm0 : lastGoodWorm1;
    remotePrevInput = (localIdx == 0) ? lastGoodWorm1 : lastGoodWorm0;

    game.setSpeculative(true);
    for (int32_t F = rollbackTo + 1; F < static_cast<int32_t>(simFrame); ++F) {
      uint32_t s = static_cast<uint32_t>(F) % INPUT_BUFFER_SIZE;
      uint8_t curLocal = localInputs[s];
      uint8_t curRemote;
      bool framePredicted;
      // Same contiguity rule as the new-frame block — only consume real
      // input (and clear the ring entry) when the chain reaches F-1.
      // Out-of-order real inputs stay in the ring for a later promote.
      bool resimContiguous = (confirmedSimFrame_ + 1 == F);
      if (remoteInputReady[s] && resimContiguous) {
        curRemote = remoteInputs[s];
        remoteInputReady[s] = false;
        lastRemoteInput_ = curRemote;
        framePredicted = false;
        confirmedSimFrame_ = F;
      } else {
        curRemote = lastRemoteInput_;
        framePredicted = true;
      }

      uint8_t risingLocal  = curLocal  & ~localPrevInput;
      uint8_t risingRemote = curRemote & ~remotePrevInput;
      uint8_t releasedLocal  = localPrevInput  & ~curLocal;
      uint8_t releasedRemote = remotePrevInput & ~curRemote;
      game.worms[localIdx ]->controlStates.istate |= risingLocal;
      game.worms[remoteIdx]->controlStates.istate |= risingRemote;
      game.worms[localIdx ]->controlStates.istate &= ~releasedLocal;
      game.worms[remoteIdx]->controlStates.istate &= ~releasedRemote;
      localPrevInput  = curLocal;
      remotePrevInput = curRemote;

      game.processFrame();

      auto& outSlot = rollbackBuffer_.write(static_cast<int>(F));
      outSlot.localInput  = (localIdx == 0) ? curLocal  : curRemote;
      outSlot.remoteInput = (localIdx == 0) ? curRemote : curLocal;
      outSlot.remoteState = framePredicted
          ? rollback::RemoteState::Predicted
          : rollback::RemoteState::Confirmed;
      game.saveSnapshotFast(outSlot.snapshot);
      outSlot.checksum = wideRollbackChecksum(game);

      // Predicted resim frames stay silent — their checksum is cached
      // for a later promote/resim pass.
      if (!framePredicted && sendChecksum) {
        sendChecksum(generation_, static_cast<uint32_t>(F), outSlot.checksum);
      }
    }
    game.setSpeculative(false);
  }

  // Stall guard: cap in-flight predicted frames at kMaxRollback so the
  // ring buffer (kMaxRollback+1 slots) can still cover the post-tick
  // window.
  if (static_cast<int32_t>(simFrame) - confirmedSimFrame_ > rollback::kMaxRollback) {
    return;
  }

  // Frame-advantage stall: hold simFrame when too far ahead of the
  // remote's last reported simFrame. The send above already ran, so the
  // remote still hears from us this tick. -1 keeps this disarmed before
  // any packet has arrived.
  if (lastKnownRemoteFrame_ >= 0 &&
      static_cast<int32_t>(simFrame) - lastKnownRemoteFrame_
          >= frameAdvantageThreshold_) {
    ++frameAdvantageStalls_;
    return;
  }

  uint32_t currentSlot = simFrame % INPUT_BUFFER_SIZE;

  uint8_t curLocal = localInputs[currentSlot];
  uint8_t curRemote;
  bool predicted;
  // Only consume real remote input when the confirmed chain reaches
  // simFrame-1. Out-of-order arrival (real input for a future frame
  // while an earlier one is still missing) must stay in the ring so a
  // later promote can fold it into a contiguous chain.
  bool chainContiguous =
      confirmedSimFrame_ + 1 == static_cast<int32_t>(simFrame);
  if (remoteInputReady[currentSlot] && chainContiguous) {
    curRemote = remoteInputs[currentSlot];
    remoteInputReady[currentSlot] = false;
    lastRemoteInput_ = curRemote;
    predicted = false;
  } else {
    curRemote = lastRemoteInput_;
    predicted = true;
  }

  uint8_t risingLocal = curLocal & ~localPrevInput;
  uint8_t risingRemote = curRemote & ~remotePrevInput;
  uint8_t releasedLocal = localPrevInput & ~curLocal;
  uint8_t releasedRemote = remotePrevInput & ~curRemote;

  game.worms[localIdx]->controlStates.istate |= risingLocal;
  game.worms[remoteIdx]->controlStates.istate |= risingRemote;
  game.worms[localIdx]->controlStates.istate &= ~releasedLocal;
  game.worms[remoteIdx]->controlStates.istate &= ~releasedRemote;

  localPrevInput = curLocal;
  remotePrevInput = curRemote;

  game.setSpeculative(predicted);
  game.processFrame();
  game.setSpeculative(false);
  ++simFrame;

  if (!predicted) {
    confirmedSimFrame_ = static_cast<int32_t>(simFrame) - 1;
  }

  // Snapshot the post-frame state and cache the checksum (used by a
  // later promote/resim if the frame was predicted).
  {
    int snapFrame = static_cast<int>(simFrame) - 1;
    rollback::Slot& slot = rollbackBuffer_.write(snapFrame);
    slot.localInput = (localIdx == 0) ? curLocal : curRemote;
    slot.remoteInput = (localIdx == 0) ? curRemote : curLocal;
    slot.remoteState = predicted
        ? rollback::RemoteState::Predicted
        : rollback::RemoteState::Confirmed;
    game.saveSnapshotFast(slot.snapshot);
    slot.checksum = wideRollbackChecksum(game);

    if (!predicted && sendChecksum) {
      sendChecksum(generation_, simFrame - 1, slot.checksum);
    }
  }

  if (game.isGameOver()) {
    state = StateGameEnded;
    if (!goingToMenu)
      enterGoingToMenu(180);
  }

  driveShadow();
}

void RollbackController::draw(Renderer& renderer, bool useSpectatorViewports) {
  if (state == StateWeaponSelection) {
    ws->draw(renderer, state, useSpectatorViewports);
  } else if (state == StateGame || state == StateGameEnded) {
    game.draw(renderer, state, useSpectatorViewports);
  }
  renderer.fadeValue = fadeValue;

  // Dev HUD: bottom-left `RB:n` resim indicator, always shown so the
  // value doesn't blink as resim windows come and go.
  if (state == StateGame) {
    Font& font = game.common->font;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "RB:%u", lastTickResimFrames_);
    font.drawText(renderer.bmp, buf, 2, renderer.renderResY - 9, 50);
  }

  if (isPaused()) {
    fill(renderer.bmp, 0);
    Common& common = *game.common;
    Font& font = common.font;
    int cx = renderer.renderResX / 2;
    int cy = renderer.renderResY / 2 - 20;

    renderer.pal = game.common->exepal;
    renderer.pal.rotateFrom(game.common->exepal, 168, 174, gfx.menuCycles);
    renderer.pal.fade(fadeValue);

    if (localPaused_) {
      std::string title = "GAME PAUSED";
      int tw = font.getDims(title);
      font.drawText(renderer.bmp, title, cx - tw / 2, cy, 50);

      pauseMenu_.place(cx, cy + 16);
      pauseMenu_.draw(common, renderer, false);
    } else {
      std::string title = "PAUSED BY PEER";
      int tw = font.getDims(title);
      font.drawText(renderer.bmp, title, cx - tw / 2, cy, 50);

      std::string hint = "PRESS ESC TO DISCONNECT";
      int hw = font.getDims(hint);
      font.drawText(renderer.bmp, hint, cx - hw / 2, cy + 16, 6);
    }
  }
}

void RollbackController::swapLevel(Level& newLevel) {
  currentLevel()->swap(newLevel);
}

Level* RollbackController::currentLevel() { return &game.level; }

Game* RollbackController::currentGame() { return &game; }

Game* RollbackController::statsGame() {
  return shadowGame_ ? shadowGame_.get() : &game;
}

bool RollbackController::running() {
  return state != StateGameEnded && state != StateInitial && resumable_;
}

void RollbackController::enterGoingToMenu(int fade) {
  goingToMenu = true;
  fadeValue = fade;
  // Without this, process()'s isPaused() early return would skip the
  // fade decrement and the menu transition would stall.
  localPaused_ = false;
  remotePaused_ = false;
}

void RollbackController::endMatch() {
  if (state == StateGame || state == StateWeaponSelection) {
    state = StateGameEnded;
    enterGoingToMenu(33);
    stopReplayRecording();
  }
}

void RollbackController::peerLeft() {
  // Mirrors the local Disconnect pause-menu branch: skip the fade and
  // do NOT transition to StateGameEnded — that keeps statsRecorder
  // unfinalized so the host loop routes us back to the menu rather
  // than to the stats screen.
  enterGoingToMenu(0);
  resumable_ = false;
  stopReplayRecording();
}
