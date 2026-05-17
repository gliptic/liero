#include "networkController.hpp"

#include "../gfx.hpp"
#include "../sfx.hpp"
#include "../mixer/player.hpp"
#include "../viewport.hpp"
#include "../spectatorviewport.hpp"

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
  localInputs.fill(0);
  remoteInputs.fill(0);
  remoteInputReady.fill(false);

  // Create worms
  for (int idx = 0; idx < 2; ++idx) {
    Worm* worm = new Worm();
    worm->settings = settings->wormSettings[idx];
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
  Worm* worm = game.findControlForKey(key, control);

  // Only accept input for the local player
  if (worm && worm->index == localIdx) {
    worm->cleanControlStates.set(control, keyState);

    if (control < Worm::MaxControl) {
      worm->setControlState(control, keyState);
    }

    if (worm->cleanControlStates[WormSettings::Dig]) {
      worm->press(Worm::Left);
      worm->press(Worm::Right);
    } else {
      if (!worm->cleanControlStates[Worm::Left])
        worm->release(Worm::Left);
      if (!worm->cleanControlStates[Worm::Right])
        worm->release(Worm::Right);
    }

    localControlState = worm->controlStates;
  }

  if (key == DkEscape && !goingToMenu) {
    fadeValue = 31;
    goingToMenu = true;
  }
}

void NetworkController::unfocus() {
  if (state == StateWeaponSelection && ws)
    ws->unfocus();
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
    game.level.generateFromSettings(*game.common, *game.settings, game.rand);
    state = StateWeaponSelection;
    ws = std::make_unique<WeaponSelection>(game);
  }
  game.focus(gfx.playRenderer);
  game.focus(gfx.singleScreenRenderer);
  goingToMenu = false;
  fadeValue = 0;
}

bool NetworkController::process() {
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

  // Apply inputs to worms so weapon selection can read them
  game.worms[localIdx]->controlStates.unpack(localInputs[currentSlot]);
  game.worms[remoteIdx]->controlStates.unpack(remoteInputs[currentSlot]);

  // Clear the slot for reuse
  remoteInputReady[currentSlot] = false;

  // Advance weapon selection
  if (ws->processFrame()) {
    // Both players are ready — finalize and start the game
    ws->finalize();
    ws.reset();

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

  // Apply inputs and advance
  game.worms[localIdx]->controlStates.unpack(localInputs[currentSlot]);
  game.worms[remoteIdx]->controlStates.unpack(remoteInputs[currentSlot]);

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
}

void NetworkController::swapLevel(Level& newLevel) {
  currentLevel()->swap(newLevel);
}

Level* NetworkController::currentLevel() { return &game.level; }

Game* NetworkController::currentGame() { return &game; }

bool NetworkController::running() {
  return state != StateGameEnded && state != StateInitial;
}
