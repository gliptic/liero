#include "networkController.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../mixer/player.hpp"
#include "../viewport.hpp"
#include "../spectatorviewport.hpp"

#include <cstring>
#include <miniz.h>

NetworkController::NetworkController(
    std::shared_ptr<Common> common,
    std::shared_ptr<Settings> settings,
    int localPlayerIdx)
    : game(common, settings,
            std::shared_ptr<SoundPlayer>(new DefaultSoundPlayer(*common)))
    , localIdx(localPlayerIdx)
    , remoteIdx(localPlayerIdx ^ 1)
    , state(StateInitial)
    , fadeValue(0)
    , goingToMenu(false)
    , simFrame(0)
    , inputDelay(3)
{
  localPrevInput = 0;
  remotePrevInput = 0;
  localHeldFrames.fill(0);
  remoteHeldFrames.fill(0);
  skipWeaponSelection = false;
  levelPreloaded = false;
  localPaused_ = false;
  remotePaused_ = false;

  // Set up pause menu
  pauseMenu_.init(true);  // centered
  pauseMenu_.addItem(MenuItem(7, 6, "RESUME", 0));
  pauseMenu_.addItem(MenuItem(7, 6, "DISCONNECT", 1));

  localInputs.fill(0);
  remoteInputs.fill(0);
  remoteInputReady.fill(false);

  // Create worms
  for (int idx = 0; idx < 2; ++idx) {
    Worm* worm = new Worm();
    // Local player uses network player profile; remote uses their slot
    worm->settings = (idx == localIdx)
        ? settings->wormSettings[Settings::NetworkPlayerIdx]
        : settings->wormSettings[idx];
    worm->health = worm->settings->health;
    worm->index = idx;
    worm->statsX = idx == 0 ? 0 : 218;
    game.addWorm(worm);
  }

  // Viewports
  game.addViewport(
      new Viewport(gvl::rect(0, 0, 158, 158), 0, 504, 350));
  game.addViewport(
      new Viewport(gvl::rect(160, 0, 158 + 160, 158), 1, 504, 350));
  game.addSpectatorViewport(
      new SpectatorViewport(gvl::rect(0, 0, 504 + 68, 350), 504, 350));
}

NetworkController::~NetworkController() {}

void NetworkController::loadLevelFromData(const std::vector<uint8_t>& data) {
  // Format: compressed_flag(1) + uncompressed_size(4) + payload
  if (data.size() < 5)
    return;

  bool isCompressed = (data[0] != 0);
  uint32_t rawSize;
  std::memcpy(&rawSize, data.data() + 1, 4);

  // Guard against decompression bombs from malicious peers
  static constexpr uint32_t MAX_RAW_SIZE = 10 * 1024 * 1024;  // 10 MB
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

  // Deserialize: width(2) + height(2) + rand_x(4) + rand_c(4) + pixel_data(w*h) + palette(768)
  if (raw.size() < 12)
    return;

  uint16_t w, h;
  std::memcpy(&w, raw.data(), 2);
  std::memcpy(&h, raw.data() + 2, 2);

  // Validate dimensions to prevent integer overflow in w*h
  if (w == 0 || h == 0 || w > 4096 || h > 4096)
    return;

  uint32_t randX, randC;
  std::memcpy(&randX, raw.data() + 4, 4);
  std::memcpy(&randC, raw.data() + 8, 4);

  size_t pixelDataSize = static_cast<size_t>(w) * h;
  if (raw.size() < 12 + pixelDataSize + 768)
    return;

  game.level.resize(w, h);
  Common& common = *game.common;

  // Load pixel data and rebuild materials
  const uint8_t* pixels = raw.data() + 12;
  for (size_t i = 0; i < pixelDataSize; ++i) {
    game.level.data[i] = pixels[i];
    game.level.materials[i] = common.materials[pixels[i]];
  }

  // Load palette
  const uint8_t* palData = raw.data() + 12 + pixelDataSize;
  for (int i = 0; i < 256; ++i) {
    game.level.origpal.entries[i].r = palData[i * 3 + 0];
    game.level.origpal.entries[i].g = palData[i * 3 + 1];
    game.level.origpal.entries[i].b = palData[i * 3 + 2];
  }

  // Restore RNG state to match host's post-generation state
  game.rand.x = randX;
  game.rand.c = randC;

  levelPreloaded = true;
}

void NetworkController::setInputCallbacks(InputSendCallback send,
                                          InputRecvCallback recv) {
  sendInput = std::move(send);
  recvInput = std::move(recv);
}

void NetworkController::injectRemoteInput(uint32_t frame, uint8_t input) {
  uint32_t slot = frame % INPUT_BUFFER_SIZE;
  remoteInputs[slot] = input;
  remoteInputReady[slot] = true;
}

void NetworkController::onKey(int key, bool keyState) {
  Worm::Control control;
  // Only check the local worm's key bindings to avoid conflicts with
  // the remote worm having the same default bindings.
  Worm* worm = game.worms[localIdx];
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

    // Update localControlState (used for network packing) independently
    // from worm->controlStates. Edge detection in advanceSimulation/
    // advanceWeaponSelection is the sole writer of worm->controlStates.
    if (control < Worm::MaxControl) {
      localControlState.set(control, keyState);
    }

    // Synthesize dig (Left+Right when Dig is held)
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
      // Already paused locally — Escape resumes
      localPaused_ = false;
      if (onLocalResume) onLocalResume();
    } else if (remotePaused_ && !goingToMenu) {
      // Remote paused, local wants to disconnect
      remotePaused_ = false;
      fadeValue = 0;
      goingToMenu = true;
    } else if (!goingToMenu) {
      // Pause the game
      localPaused_ = true;
      pauseMenu_.moveToFirstVisible();
      if (onLocalPause) onLocalPause();
    }
  }
}

void NetworkController::unfocus() {
  if (state == StateWeaponSelection && ws)
    ws->unfocus();
  localPaused_ = false;
  remotePaused_ = false;
}

void NetworkController::focus() {
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
      // Test mode: skip weapon selection, go straight to game
      for (auto* w : game.worms)
        w->initWeapons(game);
      for (auto* w : game.worms)
        w->lives = game.settings->lives;
      game.startGame();
      game.resetWorms();
      state = StateGame;
    } else {
      state = StateWeaponSelection;
      ws = std::make_unique<WeaponSelection>(game);
    }
  }
  game.focus(gfx.playRenderer);
  game.focus(gfx.singleScreenRenderer);
  goingToMenu = false;
  fadeValue = 0;
}

bool NetworkController::process() {
  if (isPaused()) {
    // While paused, don't advance simulation — just keep fade visible
    if (fadeValue < 33)
      fadeValue += 1;

    // Handle pause menu input (same as main menu navigation)
    if (localPaused_) {
      if (gfx.testSDLKeyOnce(SDL_SCANCODE_UP)
       || gfx.testControlOnce(WormSettingsExtensions::Up)
       || gfx.testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        sfx.play(*game.common, 26);
        pauseMenu_.movement(-1);
      }

      if (gfx.testSDLKeyOnce(SDL_SCANCODE_DOWN)
       || gfx.testControlOnce(WormSettingsExtensions::Down)
       || gfx.testGamepadDirOnce(SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        sfx.play(*game.common, 25);
        pauseMenu_.movement(1);
      }

      if (gfx.testSDLKeyOnce(SDL_SCANCODE_RETURN)
       || gfx.testSDLKeyOnce(SDL_SCANCODE_KP_ENTER)
       || gfx.testControlOnce(WormSettingsExtensions::Fire)
       || gfx.testGamepadButtonOnce(SDL_GAMEPAD_BUTTON_SOUTH)) {
        int sel = pauseMenu_.selectedId();
        if (sel == 0) {
          // Resume
          localPaused_ = false;
          if (onLocalResume) onLocalResume();
        } else {
          // Disconnect
          localPaused_ = false;
          fadeValue = 0;
          goingToMenu = true;
        }
      }
    }

    return true;
  }

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
        game.statsRecorder->finish(game);
      }
      return false;
    }
  } else {
    if (fadeValue < 33)
      fadeValue += 1;
  }

  return true;
}

void NetworkController::advanceWeaponSelection() {
  // Buffer the local input for this frame
  uint32_t inputFrame = simFrame + inputDelay;
  uint32_t slot = inputFrame % INPUT_BUFFER_SIZE;
  localInputs[slot] = localControlState.pack() & 0x7f;

  // Send local input via callback
  if (sendInput) {
    sendInput(inputFrame, localInputs[slot]);
  }

  // Try to get remote input via callback
  if (recvInput) {
    int result = recvInput(simFrame);
    if (result >= 0) {
      injectRemoteInput(simFrame, static_cast<uint8_t>(result));
    }
  }

  // Check if we have remote input for the current frame
  uint32_t currentSlot = simFrame % INPUT_BUFFER_SIZE;
  if (!remoteInputReady[currentSlot]) {
    return;  // Stall — waiting for remote input
  }

  // Apply inputs using edge detection — only newly pressed buttons trigger,
  // preventing held keys from auto-repeating every frame.
  uint8_t curLocal = localInputs[currentSlot];
  uint8_t curRemote = remoteInputs[currentSlot];
  uint8_t risingLocal = curLocal & ~localPrevInput;
  uint8_t risingRemote = curRemote & ~remotePrevInput;
  localPrevInput = curLocal;
  remotePrevInput = curRemote;

  game.worms[localIdx]->controlStates.unpack(risingLocal);
  game.worms[remoteIdx]->controlStates.unpack(risingRemote);

  // Clear the slot for reuse
  remoteInputReady[currentSlot] = false;

  // Advance weapon selection
  if (ws->processFrame()) {
    // Both players are ready — finalize and start the game
    ws->finalize();
    ws.reset();

    // Reset edge detection for the gameplay phase
    localPrevInput = 0;
    remotePrevInput = 0;
    localHeldFrames.fill(0);
    remoteHeldFrames.fill(0);

    for (auto* w : game.worms) {
      w->lives = game.settings->lives;
    }
    game.startGame();
    game.resetWorms();
    state = StateGame;
  }

  ++simFrame;
}

void NetworkController::advanceSimulation() {
  // Buffer the local input for this frame
  uint32_t inputFrame = simFrame + inputDelay;
  uint32_t slot = inputFrame % INPUT_BUFFER_SIZE;
  localInputs[slot] = localControlState.pack() & 0x7f;

  // Send local input via callback
  if (sendInput) {
    sendInput(inputFrame, localInputs[slot]);
  }

  // Try to get remote input via callback
  if (recvInput) {
    int result = recvInput(simFrame);
    if (result >= 0) {
      injectRemoteInput(simFrame, static_cast<uint8_t>(result));
    }
  }

  // Check if we have remote input for the current frame
  uint32_t currentSlot = simFrame % INPUT_BUFFER_SIZE;
  if (!remoteInputReady[currentSlot]) {
    // Stall — waiting for remote input
    return;
  }

  // Apply inputs preserving pressedOnce semantics with deterministic key repeat.
  // Rising edges (newly pressed) set the bit. Released bits clear it.
  // Held bits are periodically re-set to emulate key repeat (matching local SDL behavior).
  uint8_t curLocal = localInputs[currentSlot];
  uint8_t curRemote = remoteInputs[currentSlot];
  uint8_t risingLocal = curLocal & ~localPrevInput;
  uint8_t risingRemote = curRemote & ~remotePrevInput;
  uint8_t releasedLocal = localPrevInput & ~curLocal;
  uint8_t releasedRemote = remotePrevInput & ~curRemote;

  // Set newly pressed bits
  game.worms[localIdx]->controlStates.istate |= risingLocal;
  game.worms[remoteIdx]->controlStates.istate |= risingRemote;
  // Clear released bits
  game.worms[localIdx]->controlStates.istate &= ~releasedLocal;
  game.worms[remoteIdx]->controlStates.istate &= ~releasedRemote;

  // Deterministic key repeat for held bits
  for (int bit = 0; bit < 7; ++bit) {
    uint8_t mask = 1 << bit;
    // Local
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
    // Remote
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

  // Clear the slot for reuse
  remoteInputReady[currentSlot] = false;

  game.processFrame();
  ++simFrame;

  if (game.isGameOver()) {
    state = StateGameEnded;
    if (!goingToMenu) {
      fadeValue = 180;
      goingToMenu = true;
    }
  }
}

void NetworkController::draw(Renderer& renderer, bool useSpectatorViewports) {
  if (state == StateWeaponSelection) {
    ws->draw(renderer, state, useSpectatorViewports);
  } else if (state == StateGame || state == StateGameEnded) {
    game.draw(renderer, state, useSpectatorViewports);
  }
  renderer.fadeValue = fadeValue;

  // Draw pause overlay
  if (isPaused()) {
    fill(renderer.bmp, 0);
    Common& common = *game.common;
    Font& font = common.font;
    int cx = renderer.renderResX / 2;
    int cy = renderer.renderResY / 2 - 20;

    if (localPaused_) {
      std::string title = "GAME PAUSED";
      int tw = font.getDims(title);
      font.drawText(renderer.bmp, title, cx - tw / 2, cy, 50);

      pauseMenu_.place(cx, cy + 16);
      pauseMenu_.draw(common, renderer, false);
    } else {
      // Remote paused — show info and disconnect option
      std::string title = "PAUSED BY PEER";
      int tw = font.getDims(title);
      font.drawText(renderer.bmp, title, cx - tw / 2, cy, 50);

      std::string hint = "PRESS ESC TO DISCONNECT";
      int hw = font.getDims(hint);
      font.drawText(renderer.bmp, hint, cx - hw / 2, cy + 16, 6);
    }
  }
}

void NetworkController::swapLevel(Level& newLevel) {
  currentLevel()->swap(newLevel);
}

Level* NetworkController::currentLevel() { return &game.level; }

Game* NetworkController::currentGame() { return &game; }

bool NetworkController::running() {
  return state != StateGameEnded && state != StateInitial;
}
