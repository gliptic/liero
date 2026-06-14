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
    std::function<void(uint8_t generation, uint32_t base_frame, uint8_t count,
                       uint8_t const* inputs, uint32_t local_frame)>;

// Checksum emission for desync detection. `generation` = sender's
// phase generation.
using ChecksumSendCallback =
    std::function<void(uint8_t generation, uint32_t frame, uint32_t checksum)>;

struct RollbackController : CommonController {
  RollbackController(const std::shared_ptr<Common>& common,
                     const std::shared_ptr<Settings>& settings, int local_player_idx);
  ~RollbackController() override;

  void OnKey(int key, bool key_state) override;
  void Unfocus() override;
  void Focus() override;
  bool Process() override;
  void Draw(Renderer& renderer, bool use_spectator_viewports) override;
  void SwapLevel(Level& new_level) override;
  Level* CurrentLevel() override;
  Game* CurrentGame() override;
  Game* StatsGame() override;
  bool Running() override;

  void SetInputCallbacks(InputBatchSendCallback send);
  void SetChecksumCallback(ChecksumSendCallback cb) { sendChecksum_ = std::move(cb); }

  void InjectRemoteInput(uint32_t frame, uint8_t input);

  // `generation` is the sender's phase generation. Batches from an older
  // generation are dropped (their simFrame numbering describes a phase
  // the local controller has already abandoned, and slots are reused
  // across the phase boundary). Batches from generation_+1 are buffered
  // until the local phase transition fires; anything further is dropped.
  void InjectRemoteBatch(uint8_t generation, uint32_t base_frame, uint8_t count,
                         uint8_t const* inputs, uint32_t remote_local_frame);
  // Same-generation overload for tests that aren't exercising the
  // wire-level generation filter.
  void InjectRemoteBatch(uint32_t base_frame, uint8_t count, uint8_t const* inputs,
                         uint32_t remote_local_frame) {
    InjectRemoteBatch(generation_, base_frame, count, inputs, remote_local_frame);
  }

  void SetRemotePaused(bool paused) { remotePaused_ = paused; }
  bool IsPaused() const { return localPaused_ || remotePaused_; }

  void SetPauseCallbacks(std::function<void()> pause_cb, std::function<void()> resume_cb) {
    onLocalPause_ = std::move(pause_cb);
    onLocalResume_ = std::move(resume_cb);
  }

  void SetEndMatchCallback(std::function<void()> cb) { onEndMatch_ = std::move(cb); }
  void SetPeerLeftCallback(std::function<void()> cb) { onPeerLeft_ = std::move(cb); }

  // Test/embedding hook: redirect the multiplayer replay writer to a
  // user-supplied io::Writer instead of the gfx-configured Replays/
  // directory. Must be called before focus() so it's in place when
  // setupShadowGame runs. Overrides the recordReplays/gfx gates.
  void SetReplayWriterOverride(std::unique_ptr<io::Writer> sink) {
    replayWriterOverride_ = std::move(sink);
  }

  // Test accessor: the shadow Game tracking confirmed frames. Used by
  // the replay round-trip test to compare the shadow's final state
  // against the replayed file's playback state.
  Game* ShadowGameForTest() { return shadowGame_.get(); }
  // Single entry into the goingToMenu fade. Clears pause flags so
  // process()'s paused-branch early-return can't strand fadeValue.
  void EnterGoingToMenu(int fade);
  void EndMatch();
  // Used by both the local "Disconnect" pause menu option and the wire
  // PeerLeft handler: drop to the menu without finalizing stats.
  void PeerLeft();
  // Mark the controller as no longer resumable. Called when the
  // underlying NetSession has gone away (clean PeerLeft or socket
  // close); makes `running()` return false so the main menu hides
  // "RESUME GAME".
  void MarkUnresumable() override { resumable_ = false; }

  void SetSkipWeaponSelection(bool skip) { skipWeaponSelection_ = skip; }

  void LoadLevelFromData(const std::vector<uint8_t>& data);
  void SetLevelPreloaded() { levelPreloaded_ = true; }

  uint32_t CurrentFrame() const { return simFrame_; }
  GameState State() const { return state_; }
  bool InWeaponSelection() override { return state_ == kStateWeaponSelection; }
  void SetLocalControlState(uint8_t packed) { localControlState_.Unpack(packed); }
  // Must be called before the first sim tick. Clamped to kMaxRollback:
  // the send path encodes (localFrame - baseFrame) as a uint8_t equal to
  // (K-1) - inputDelay, which underflows once inputDelay exceeds K-1.
  void SetInputDelay(uint32_t frames) {
    inputDelay_ =
        frames > rollback::kMaxRollback ? static_cast<uint32_t>(rollback::kMaxRollback) : frames;
  }

  rollback::RollbackBuffer const& RollbackBuffer() const { return rollbackBuffer_; }

  // Highest simFrame run with real (received) remote input; anything
  // past this is currently a prediction. -1 before the first frame.
  int32_t ConfirmedFrame() const { return confirmedSimFrame_; }

  uint64_t RollbackCount() const { return rollbackCount_; }

  // Frames the resim loop replayed during the most recent process() tick.
  // Reset each tick. Used by the dev HUD overlay (`RB:n`).
  uint32_t LastTickResimFrames() const { return lastTickResimFrames_; }

  // Sender-side simFrame from the most recent batched packet accepted.
  // -1 before any packet arrives, so the frame-advantage stall stays
  // disarmed during warm-up.
  int32_t LastKnownRemoteFrame() const { return lastKnownRemoteFrame_; }
  uint64_t FrameAdvantageStallCount() const { return frameAdvantageStalls_; }

  // Phase generation. 0 = weapon select, 1 = game, etc. Bumped at every
  // WS→game transition so the wire layer can drop pre-transition packets.
  uint8_t Generation() const { return generation_; }
  void SetGenerationForTest(uint8_t g) { generation_ = g; }
  void ResetForGamePhaseForTest() { ResetForGamePhase(); }
  uint64_t DroppedOldGenerationBatches() const { return droppedOldGenerationBatches_; }
  uint8_t PendingFutureBatchCount() const { return pendingFutureCount_; }

  // Stall a tick when simFrame is at least this far ahead of the remote's
  // last reported simFrame. 5 absorbs natural ±1-2 frame jitter between
  // two independent 70 fps processes (lower values caused a ~25% stall
  // rate on a quiet loopback link) while still leaving 2 frames of headroom
  // before the kMaxRollback=7 stall fires.
  static constexpr int32_t kFrameAdvantage = 5;

  // Test hook: raises the threshold high enough that the stall never
  // fires, so tests exercising loss/reorder can freely run ahead.
  void SetFrameAdvantageEnabled(bool enabled) {
    frameAdvantageThreshold_ = enabled ? kFrameAdvantage : INT32_MAX;
  }

  Game game;

  // Save / restore mutable state touched during weapon selection. Public
  // so tests can drive a round-trip. worm.weapons[].type and menu item
  // display strings are re-derived on restore from the snapshotted
  // weapon IDs via Common.
  void SaveWeaponSelectSnap(WeaponSelectSnap& snap) const;
  void LoadWeaponSelectSnap(WeaponSelectSnap const& snap);

 private:
  void AdvanceSimulation();
  void AdvanceWeaponSelection();
  // Apply (curLocal, curRemote) to worm control states, run one ws tick,
  // and return whether weapon selection is now complete. Shared between
  // the forward path and the rollback resim.
  bool WeaponSelectStep(uint8_t cur_local, uint8_t cur_remote);
  // Idempotent via the state check.
  void FinishWeaponSelect();
  void SendInputWindow(uint32_t newest_frame, uint32_t local_frame);
  // Full controller state reset for a phase transition. Clears the
  // input ring, snapshot ring, frame counters, and edge-detection state;
  // bumps generation_.
  void ResetForGamePhase();

  int localIdx_;
  int remoteIdx_;

  ::GameState state_{kStateInitial};
  // True once the game phase actually started (startGame ran). Gates
  // StateGameEnded ticks in process(): END MATCH during weapon
  // selection enters StateGameEnded without a started Game, and the
  // game-phase sim must not run on it.
  bool gamePhaseEntered_{false};
  int fadeValue_{0};
  bool goingToMenu_{false};

  uint32_t simFrame_{0};
  uint32_t inputDelay_{3};
  // simFrame + inputDelay of the most recent local input we packed into
  // localInputs[]. Empty (no input packed yet this phase) when false.
  uint32_t lastSentFrame_{0};
  bool lastSentFrameValid_{false};

  static constexpr uint32_t kInputBufferSize = 256;
  std::array<uint8_t, kInputBufferSize> localInputs_;
  std::array<uint8_t, kInputBufferSize> remoteInputs_;
  std::array<bool, kInputBufferSize> remoteInputReady_;

  Worm::ControlState localControlState_;

  uint8_t localPrevInput_{0};
  uint8_t remotePrevInput_{0};

  static constexpr int kKeyRepeatInitial = 12;
  static constexpr int kKeyRepeatInterval = 3;
  std::array<uint16_t, 8> localHeldFrames_;
  std::array<uint16_t, 8> remoteHeldFrames_;

  bool skipWeaponSelection_{false};
  bool levelPreloaded_{false};

  bool localPaused_{false};
  bool remotePaused_{false};
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

  void SetupShadowGame();
  void StartReplayRecording();
  void StopReplayRecording();
  void DriveShadow();

  // Prepare the rollback ring (idempotent), seed slot 0 with the
  // post-startGame state, then construct the shadow Game. Shared by
  // the WS->Game transition (finishWeaponSelect) and the skip-WS path
  // (focus's StateInitial branch).
  void SeedRollbackAndShadow();

  InputBatchSendCallback sendInputBatch_;
  ChecksumSendCallback sendChecksum_;

  std::function<void()> onLocalPause_;
  std::function<void()> onLocalResume_;
  std::function<void()> onEndMatch_;
  std::function<void()> onPeerLeft_;

  std::unique_ptr<WeaponSelection> ws_;

  // Snapshots after each confirmed frame, plus the local/remote input
  // bytes that produced them. Pre-sized in focus() once the level is
  // generated.
  rollback::RollbackBuffer rollbackBuffer_;
  bool rollbackBufferPrepared_{false};

  // Prediction state.
  // confirmedSimFrame_ — highest simFrame already advanced whose remote
  //   input was the real (received) byte. -1 before the first frame.
  // lastRemoteInput_ — most recent real remote input byte; used as the
  //   prediction when remote input for the current frame is missing
  //   (GGPO-style input duplication).
  int32_t confirmedSimFrame_{-1};
  uint8_t lastRemoteInput_{0};

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
    uint32_t base_frame;
    uint8_t count;
    std::array<uint8_t, rollback::kMaxRollback + 1> inputs;
    uint32_t remote_local_frame;
  };
  std::array<PendingFutureBatch, kMaxPendingFutureBatches> pendingFutureBatches_{};
  uint8_t pendingFutureCount_ = 0;
};
