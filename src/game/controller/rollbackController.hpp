#pragma once

#include <array>
#include <climits>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "../game.hpp"
#include "../io/stream.hpp"
#include "../menu/menu.hpp"
#include "../rollback/buffer.hpp"
#include "../weapsel.hpp"
#include "../worm.hpp"
#include "commonController.hpp"

struct ReplayWriter;

// Batched input send: emits the last K = kMaxRollback + 1 local inputs
// per tick so a dropped packet is covered by the next K-1.
// `localFrame` = sender's simFrame at send time (frame-advantage
// tracking); `generation` = sender's phase generation (receivers drop
// stale ones).
using InputBatchSendCallback =
    std::function<void(uint8_t generation, uint32_t baseFrame, uint8_t count, uint8_t const* inputs,
                       uint32_t localFrame)>;

// Checksum emission for desync detection. `generation` = sender's
// phase generation.
using ChecksumSendCallback =
    std::function<void(uint8_t generation, uint32_t frame, uint32_t checksum)>;

struct RollbackController : CommonController {
  RollbackController(std::shared_ptr<Common> common, std::shared_ptr<Settings> settings,
                     int localPlayerIdx);
  ~RollbackController();

  void onKey(int key, bool keyState) override;
  void unfocus() override;
  void focus() override;
  bool process() override;
  void draw(Renderer& renderer, bool useSpectatorViewports) override;
  void swapLevel(Level& newLevel) override;
  Level* currentLevel() override;
  Game* currentGame() override;
  Game* statsGame() override;
  bool running() override;

  void setInputCallbacks(InputBatchSendCallback send);
  void setChecksumCallback(ChecksumSendCallback cb) { sendChecksum = std::move(cb); }

  void injectRemoteInput(uint32_t frame, uint8_t input);

  // `generation` is the sender's phase generation. Batches from an older
  // generation are dropped (their simFrame numbering describes a phase
  // the local controller has already abandoned, and slots are reused
  // across the phase boundary). Batches from generation_+1 are buffered
  // until the local phase transition fires; anything further is dropped.
  void injectRemoteBatch(uint8_t generation, uint32_t baseFrame, uint8_t count,
                         uint8_t const* inputs, uint32_t remoteLocalFrame);
  // Same-generation overload for tests that aren't exercising the
  // wire-level generation filter.
  void injectRemoteBatch(uint32_t baseFrame, uint8_t count, uint8_t const* inputs,
                         uint32_t remoteLocalFrame) {
    injectRemoteBatch(generation_, baseFrame, count, inputs, remoteLocalFrame);
  }

  void setRemotePaused(bool paused) { remotePaused_ = paused; }
  bool isPaused() const { return localPaused_ || remotePaused_; }

  void setPauseCallbacks(std::function<void()> pauseCb, std::function<void()> resumeCb) {
    onLocalPause = std::move(pauseCb);
    onLocalResume = std::move(resumeCb);
  }

  void setEndMatchCallback(std::function<void()> cb) { onEndMatch = std::move(cb); }
  void setPeerLeftCallback(std::function<void()> cb) { onPeerLeft = std::move(cb); }

  // Test/embedding hook: redirect the multiplayer replay writer to a
  // user-supplied io::Writer instead of the gfx-configured Replays/
  // directory. Must be called before focus() so it's in place when
  // setupShadowGame runs. Overrides the recordReplays/gfx gates.
  void setReplayWriterOverride(std::unique_ptr<io::Writer> sink) {
    replayWriterOverride_ = std::move(sink);
  }

  // Test accessor: the shadow Game tracking confirmed frames. Used by
  // the replay round-trip test to compare the shadow's final state
  // against the replayed file's playback state.
  Game* shadowGameForTest() { return shadowGame_.get(); }
  // Single entry into the goingToMenu fade. Clears pause flags so
  // process()'s paused-branch early-return can't strand fadeValue.
  void enterGoingToMenu(int fade);
  void endMatch();
  // Used by both the local "Disconnect" pause menu option and the wire
  // PeerLeft handler: drop to the menu without finalizing stats.
  void peerLeft();
  // Mark the controller as no longer resumable. Called when the
  // underlying NetSession has gone away (clean PeerLeft or socket
  // close); makes `running()` return false so the main menu hides
  // "RESUME GAME".
  void markUnresumable() { resumable_ = false; }

  void setSkipWeaponSelection(bool skip) { skipWeaponSelection = skip; }

  void loadLevelFromData(const std::vector<uint8_t>& data);
  void setLevelPreloaded() { levelPreloaded = true; }

  uint32_t currentFrame() const { return simFrame; }
  GameState gameState() const { return state; }
  void setLocalControlState(uint8_t packed) { localControlState.unpack(packed); }
  // Must be called before the first sim tick. Clamped to kMaxRollback:
  // the send path encodes (localFrame - baseFrame) as a uint8_t equal to
  // (K-1) - inputDelay, which underflows once inputDelay exceeds K-1.
  void setInputDelay(uint32_t frames) {
    inputDelay =
        frames > rollback::kMaxRollback ? static_cast<uint32_t>(rollback::kMaxRollback) : frames;
  }

  rollback::RollbackBuffer const& rollbackBuffer() const { return rollbackBuffer_; }

  // Highest simFrame run with real (received) remote input; anything
  // past this is currently a prediction. -1 before the first frame.
  int32_t confirmedFrame() const { return confirmedSimFrame_; }

  uint64_t rollbackCount() const { return rollbackCount_; }

  // Frames the resim loop replayed during the most recent process() tick.
  // Reset each tick. Used by the dev HUD overlay (`RB:n`).
  uint32_t lastTickResimFrames() const { return lastTickResimFrames_; }

  // Sender-side simFrame from the most recent batched packet accepted.
  // -1 before any packet arrives, so the frame-advantage stall stays
  // disarmed during warm-up.
  int32_t lastKnownRemoteFrame() const { return lastKnownRemoteFrame_; }
  uint64_t frameAdvantageStallCount() const { return frameAdvantageStalls_; }

  // Phase generation. 0 = weapon select, 1 = game, etc. Bumped at every
  // WS→game transition so the wire layer can drop pre-transition packets.
  uint8_t generation() const { return generation_; }
  void setGenerationForTest(uint8_t g) { generation_ = g; }
  void resetForGamePhaseForTest() { resetForGamePhase(); }
  uint64_t droppedOldGenerationBatches() const { return droppedOldGenerationBatches_; }
  uint8_t pendingFutureBatchCount() const { return pendingFutureCount_; }

  // Stall a tick when simFrame is at least this far ahead of the remote's
  // last reported simFrame. 5 absorbs natural ±1-2 frame jitter between
  // two independent 70 fps processes (lower values caused a ~25% stall
  // rate on a quiet loopback link) while still leaving 2 frames of headroom
  // before the kMaxRollback=7 stall fires.
  static constexpr int32_t kFrameAdvantage = 5;

  // Test hook: raises the threshold high enough that the stall never
  // fires, so tests exercising loss/reorder can freely run ahead.
  void setFrameAdvantageEnabled(bool enabled) {
    frameAdvantageThreshold_ = enabled ? kFrameAdvantage : INT32_MAX;
  }

  Game game;

 public:
  // Save / restore mutable state touched during weapon selection. Public
  // so tests can drive a round-trip. worm.weapons[].type and menu item
  // display strings are re-derived on restore from the snapshotted
  // weapon IDs via Common.
  void saveWeaponSelectSnap(WeaponSelectSnap& snap) const;
  void loadWeaponSelectSnap(WeaponSelectSnap const& snap);

 private:
  void advanceSimulation();
  void advanceWeaponSelection();
  // Apply (curLocal, curRemote) to worm control states, run one ws tick,
  // and return whether weapon selection is now complete. Shared between
  // the forward path and the rollback resim.
  bool weaponSelectStep(uint8_t curLocal, uint8_t curRemote);
  // Idempotent via the state check.
  void finishWeaponSelect();
  void sendInputWindow(uint32_t newestFrame, uint32_t localFrame);
  // Full controller state reset for a phase transition. Clears the
  // input ring, snapshot ring, frame counters, and edge-detection state;
  // bumps generation_.
  void resetForGamePhase();

  int localIdx;
  int remoteIdx;

  GameState state;
  int fadeValue;
  bool goingToMenu;

  uint32_t simFrame;
  uint32_t inputDelay;
  // simFrame + inputDelay of the most recent local input we packed into
  // localInputs[]. Empty (no input packed yet this phase) when false.
  uint32_t lastSentFrame;
  bool lastSentFrameValid;

  static constexpr uint32_t INPUT_BUFFER_SIZE = 256;
  std::array<uint8_t, INPUT_BUFFER_SIZE> localInputs;
  std::array<uint8_t, INPUT_BUFFER_SIZE> remoteInputs;
  std::array<bool, INPUT_BUFFER_SIZE> remoteInputReady;

  Worm::ControlState localControlState;

  uint8_t localPrevInput;
  uint8_t remotePrevInput;

  static constexpr int KEY_REPEAT_INITIAL = 12;
  static constexpr int KEY_REPEAT_INTERVAL = 3;
  std::array<uint16_t, 8> localHeldFrames;
  std::array<uint16_t, 8> remoteHeldFrames;

  bool skipWeaponSelection;
  bool levelPreloaded;

  bool localPaused_;
  bool remotePaused_;
  bool resumable_ = true;
  Menu pauseMenu_;

  // Shadow Game driven only by confirmed frames (never predicted,
  // never resimmed). Hosts the player-facing NormalStatsRecorder (the
  // live game's is replaced with a no-op since its processFrame fires
  // speculatively) and feeds a ReplayWriter so multiplayer matches
  // produce .lrp recordings. Owning a second Game would double sim
  // cost in the worst case; we only advance it as confirmations land,
  // so steady-state cost is one extra processFrame per real tick (no
  // rollback amplification).
  std::unique_ptr<Game> shadowGame_;
  std::unique_ptr<ReplayWriter> shadowReplay_;
  std::unique_ptr<io::Writer> replayWriterOverride_;
  int32_t shadowFrame_ = -1;
  uint8_t shadowLocalPrevInput_ = 0;
  uint8_t shadowRemotePrevInput_ = 0;
  bool shadowMismatchLogged_ = false;

  void setupShadowGame();
  void startReplayRecording();
  void stopReplayRecording();
  void driveShadow();

  // Prepare the rollback ring (idempotent), seed slot 0 with the
  // post-startGame state, then construct the shadow Game. Shared by
  // the WS->Game transition (finishWeaponSelect) and the skip-WS path
  // (focus's StateInitial branch).
  void seedRollbackAndShadow();

  InputBatchSendCallback sendInputBatch;
  ChecksumSendCallback sendChecksum;

  std::function<void()> onLocalPause;
  std::function<void()> onLocalResume;
  std::function<void()> onEndMatch;
  std::function<void()> onPeerLeft;

  std::unique_ptr<WeaponSelection> ws;

  // Snapshots after each confirmed frame, plus the local/remote input
  // bytes that produced them. Pre-sized in focus() once the level is
  // generated.
  rollback::RollbackBuffer rollbackBuffer_;
  bool rollbackBufferPrepared_;

  // Prediction state.
  // confirmedSimFrame_ — highest simFrame already advanced whose remote
  //   input was the real (received) byte. -1 before the first frame.
  // lastRemoteInput_ — most recent real remote input byte; used as the
  //   prediction when remote input for the current frame is missing
  //   (GGPO-style input duplication).
  int32_t confirmedSimFrame_;
  uint8_t lastRemoteInput_;

  uint64_t rollbackCount_ = 0;
  uint32_t lastTickResimFrames_ = 0;

  // Monotonic; stale packets carrying smaller frames are ignored.
  int32_t lastKnownRemoteFrame_ = -1;
  uint64_t frameAdvantageStalls_ = 0;
  int32_t frameAdvantageThreshold_ = kFrameAdvantage;

  // 0 until the first WS→game transition increments it.
  uint8_t generation_ = 0;
  uint64_t droppedOldGenerationBatches_ = 0;

  // Bounded queue for batches arriving from a peer that has already
  // crossed the next phase boundary while we haven't yet. Capacity
  // matches the K-wide redundancy so one full peer-side resend fits.
  //
  // Stall budget: at 70 Hz the peer emits one batch per tick, so 8 slots
  // buffers ~115 ms of "peer ahead" before we start dropping. The local
  // phase transition is driven by the same confirmed-frame stream and
  // typically fires within 1–2 ticks of the peer's, so this is well
  // above the observed worst case.
  static constexpr uint8_t kMaxPendingFutureBatches =
      static_cast<uint8_t>(rollback::kMaxRollback + 1);
  struct PendingFutureBatch {
    uint32_t baseFrame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t remoteLocalFrame;
  };
  std::array<PendingFutureBatch, kMaxPendingFutureBatches> pendingFutureBatches_{};
  uint8_t pendingFutureCount_ = 0;
};
