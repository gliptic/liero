#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "commonController.hpp"
#include "../game.hpp"
#include "../worm.hpp"
#include "../weapsel.hpp"

// Callback type for sending local input to the transport layer.
// Called each frame with: (frame_number, local_input_byte)
using InputSendCallback = std::function<void(uint32_t frame, uint8_t input)>;

// Callback type for requesting remote input from the transport layer.
// Called each frame with: (frame_number) -> input byte, or -1 if not yet available
using InputRecvCallback = std::function<int(uint32_t frame)>;

struct NetworkController : CommonController {
  NetworkController(std::shared_ptr<Common> common,
                    std::shared_ptr<Settings> settings,
                    int localPlayerIdx);
  ~NetworkController();

  void onKey(int key, bool keyState) override;
  void unfocus() override;
  void focus() override;
  bool process() override;
  void draw(Renderer& renderer, bool useSpectatorViewports) override;
  void swapLevel(Level& newLevel) override;
  Level* currentLevel() override;
  Game* currentGame() override;
  bool running() override;

  // Set the callbacks for input transport
  void setInputCallbacks(InputSendCallback send, InputRecvCallback recv);

  // Directly inject inputs for a given frame (for testing without network)
  void injectRemoteInput(uint32_t frame, uint8_t input);

  // Get current simulation frame number
  uint32_t currentFrame() const { return simFrame; }

  Game game;

 private:
  void advanceSimulation();
  void advanceWeaponSelection();

  int localIdx;    // 0 or 1 — which worm is the local player
  int remoteIdx;   // the other worm

  GameState state;
  int fadeValue;
  bool goingToMenu;

  uint32_t simFrame;       // Current simulation frame number
  uint32_t inputDelay;     // Frames of input delay (default: 3)

  // Input buffers: frame -> input byte
  static constexpr uint32_t INPUT_BUFFER_SIZE = 256;
  std::array<uint8_t, INPUT_BUFFER_SIZE> localInputs;
  std::array<uint8_t, INPUT_BUFFER_SIZE> remoteInputs;
  std::array<bool, INPUT_BUFFER_SIZE> remoteInputReady;

  // Current local control state (from keyboard)
  Worm::ControlState localControlState;

  InputSendCallback sendInput;
  InputRecvCallback recvInput;

  // Weapon selection (active during StateWeaponSelection)
  std::unique_ptr<WeaponSelection> ws;
};
