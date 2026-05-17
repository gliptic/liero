# Multiplayer for Open Liero

## Problem Statement

How might we enable two-player networked multiplayer in Open Liero while preserving 100% gameplay fidelity with the local experience?

## Recommended Direction: Lockstep Deterministic (with rollback upgrade path)

The game's replay system already implements lockstep: deterministic sim + per-frame input deltas. Multiplayer is essentially "replay recording/playback, but the remote player's inputs come from the network instead of a file."

**Phase 1 (Alpha):** Pure lockstep with fixed input delay (2-4 frames, ~28-56ms). Players exchange XOR-compressed input bitfields each frame over UDP. Both peers run identical simulations. Include periodic checksums to detect desync. Target: LAN and low-latency internet (<60ms RTT).

**Phase 2 (Internet-quality):** Add GGPO-style rollback. Local input applied immediately; remote input predicted. On mismatch, snapshot/restore/resimulate. The fixed-size object lists (`ExactObjectList<T, N>`) are ideal for fast memcpy snapshots.

### Why this works for Open Liero specifically:

- **Fixed-point math throughout** — no cross-platform floating-point divergence
- **Replay system proves determinism** — input deltas + seed already reproduce games
- **Tiny input state** — `controlStates` is a single byte per worm per frame
- **Fixed-size object pools** — predictable memory layout for fast state snapshots
- **`postClone()` already exists** — state copying infrastructure is partially there

## Key Assumptions to Validate

- [x] **Sim is fully deterministic given same seed + inputs** — Verified! Test harness runs two identical Game instances with random inputs for 1000 frames and confirms byte-identical state after each frame. No desync detected.
- [ ] **No AI needed in network games** — If multiplayer is human-only, threaded AI non-determinism is irrelevant. If AI is needed, it must be made single-threaded/deterministic.
- [ ] **State serialization is complete** — The replay serialization captures all sim-affecting state. Verify by: save state, advance N frames, restore state, advance N frames again → must match.
- [ ] **UDP hole-punching is feasible for later** — For alpha, direct connection is fine. For public release, NAT traversal (STUN/TURN or relay) is needed.

## MVP Scope (Alpha)

### In scope:

- `NetworkController` — new controller type that exchanges inputs over UDP
- Determinism audit + test harness (run two instances, verify frame-by-frame checksum match)
- Fixed input delay buffer (configurable, default 3 frames)
- Direct IP connection (one player hosts, other connects)
- Connection setup UI (host/join with IP:port)
- Periodic desync detection (checksum every N frames, disconnect on mismatch)
- Human vs Human only (no AI in network games)

### Architecture:

```
┌──────────────┐         UDP          ┌──────────────┐
│   Peer A     │◄────────────────────►│   Peer B     │
│              │   input deltas +     │              │
│ LocalInput   │   frame numbers +    │ LocalInput   │
│     ↓        │   checksums          │     ↓        │
│ InputBuffer  │                      │ InputBuffer  │
│     ↓        │                      │     ↓        │
│ Game::       │                      │ Game::       │
│ processFrame │ (identical sim)      │ processFrame │
└──────────────┘                      └──────────────┘
```

### Implementation steps:

1. ~~**Determinism harness**~~ ✅ — Run two `Game` instances in-process with same seed/inputs, assert state matches after every frame
2. ~~**NetworkController**~~ ✅ — Implements `Controller` interface; buffers local+remote inputs; advances sim when both available
3. ~~**UDP transport**~~ ✅ — ENet-based reliable UDP transport with input, handshake, and checksum packet types
4. ~~**Connection flow**~~ ✅ — `NetSession` wires controller + transport: handshake exchange, seed sync, settings validation
5. ~~**UI**~~ ✅ — HOST GAME / JOIN GAME menu options, IP address input, connection status screen
6. ~~**Weapon selection + settings sync**~~ ✅ — Integrated weapon selection screen, synced RNG, player info exchange, host-authoritative match settings, edge detection for pressedOnce semantics

## How to Play

1. Both players configure match settings (Match Setup) before connecting
2. Player A: select **HOST GAME** from the main menu — shows "HOSTING ON PORT 19532"
3. Player B: select **JOIN GAME**, type the host's IP address, press Enter
4. Both see connection status, then the game starts automatically
5. Press Escape during gameplay to disconnect and return to menu

## Not Doing (and Why)

- **Rollback/prediction** — Adds massive complexity; lockstep with delay is sufficient for alpha and LAN play
- **More than 2 players** — The original game is 2-player; network protocol can be designed to allow expansion later but not implementing now
- **Matchmaking server** — Alpha uses direct IP connection; relay/STUN comes later
- **Spectator mode** — Nice-to-have, not core
- **Web/Emscripten multiplayer** — WebRTC adds complexity; desktop-only for now
- **AI in network games** — Threading non-determinism makes this hard; punt to later
- **Replay recording of network games** — Could be added later since we're already exchanging the right data, but not in alpha
- **Encryption/authentication** — Trust the peer for now; add later if needed

## Learnings

### Determinism (validated 2026-05-17)

The game simulation is **fully deterministic** given the same seed and inputs. Key factors:
- All game physics use fixed-point arithmetic (`fixedvec`), no floating-point in the sim path
- The PRNG (`gvl::mwc`) is a simple multiply-with-carry with two uint32 values (x, c)
- Object lists use fixed-size pools with deterministic iteration order
- The `processFrame()` function processes all entities in a fixed order

The test harness (`src/tests/test_determinism.cpp`) runs two `Game` instances with identical
seed and random inputs for 1000 frames and verifies state hashes match every frame.

**Important setup note:** Worms must have `initWeapons()` called before the game starts,
otherwise weapon type pointers are null and the sim will crash in `processSteerables()`.

### NetworkController design (2026-05-17)

The `NetworkController` (`src/game/controller/networkController.hpp`) implements the existing
`Controller` interface. Key design decisions:

- **Separation of concerns:** The controller itself has no socket code. It communicates via
  `InputSendCallback` and `InputRecvCallback` function objects, plus a `injectRemoteInput()`
  method for direct injection. This makes it testable without a network.
- **Input delay buffer:** A ring buffer of 256 slots. The local player's input is written at
  `simFrame + inputDelay` (default 3 frames ahead). The sim only advances when both local and
  remote inputs are available for `simFrame`.
- **Stalling:** If remote input isn't available, the controller simply doesn't advance. No
  prediction, no interpolation. This is the lockstep guarantee.
- **Local-only input:** `onKey()` only accepts keys for the local player's worm index.
  The remote player's inputs come exclusively via `injectRemoteInput()`.
- **Game initialization:** Currently skips weapon selection and uses default weapons. For the
  full implementation, both peers will need to exchange weapon selections before starting.

**Architecture for next steps:** The UDP transport will be a separate class that owns the socket
and calls `injectRemoteInput()` on the controller. This keeps networking concerns out of the
game logic.

### Answered open questions

- **PRNG state:** `gvl::mwc` has exactly two uint32 fields (`x` and `c`). Synchronizing the
  seed before game start is sufficient — verified by tests showing both games maintain identical
  `rand.x` and `rand.c` after hundreds of frames.
- **Input delay already integrated:** The 3-frame buffer is part of the NetworkController.
  The next step is just the wire protocol (frame number + input byte over UDP).
- **UDP library:** Using [zpl-c/enet](https://github.com/zpl-c/enet) (v2.6.5), the IPv6-enabled
  fork of ENet. Added as a vcpkg overlay port in `tools/vcpkg/overlay-ports/enet/`. Provides
  reliable ordered delivery on channel 0 (inputs, handshake) and unreliable delivery on
  channel 1 (checksums). The library is a single-header C implementation — very lightweight.

### Transport design (2026-05-17)

The `NetTransport` (`src/game/net/transport.hpp`) wraps ENet and provides:

- **Three packet types:** `PacketInput` (6 bytes: type + frame + input), `PacketHandshake`
  (9 bytes: type + seed + settingsHash), `PacketChecksum` (9 bytes: type + frame + checksum)
- **Two ENet channels:** Channel 0 = reliable ordered (inputs, handshake), Channel 1 =
  unreliable (checksums — losing one is fine)
- **Callback-based:** `onRemoteInput`, `onHandshake`, `onChecksum`, `onConnected`,
  `onDisconnected` callbacks drive the controller without polling
- **Non-blocking poll:** `poll()` returns immediately; call it once per game frame
- **IPv6 ready:** Uses zpl-c/enet which supports dual-stack (IPv4 + IPv6)

**Wire format is intentionally simple and fixed-size** — no varints, no serialization
frameworks. A frame input is exactly 6 bytes on the wire. At 70fps, that's ~420 bytes/sec
per direction — essentially zero bandwidth.

### Connection flow / NetSession (2026-05-17)

The `NetSession` (`src/game/net/session.hpp`) wires `NetworkController` and `NetTransport`
together. It manages the full connection lifecycle:

- **State machine:** Idle → WaitingForPeer → Handshaking → Playing → Disconnected/Failed
- **Role assignment:** Host = player 0, Client = player 1
- **Handshake protocol:** Both peers send a handshake packet containing a seed (host-
  authoritative, client sends 0) and a settings hash. If hashes don't match, the session
  moves to Failed state — prevents playing with incompatible settings.
- **Settings hash:** FNV-1a hash of gameplay-affecting settings (lives, loading time,
  game mode, blood, max bonuses, time to lose, flags to win, load change).
- **RNG sync:** The host generates a random seed from `std::time()`. After handshake
  completes, both peers `game.rand.seed(gameSeed_)` before starting the game, ensuring
  identical simulation state.
- **Callback wiring:** Transport's `onRemoteInput` → controller's `injectRemoteInput()`;
  controller's `sendInput` callback → transport's `sendInput()`.
- **Controller release:** `NetSession::releaseController()` transfers ownership of the
  NetworkController to `Gfx::controller` while keeping a raw pointer (`controllerPtr_`)
  for remote input injection. This lets the session continue polling the transport while
  the controller is owned by the main game loop.
- **Tested end-to-end:** Session tests verify handshake completion, settings mismatch
  rejection, and actual frame-advancing over real localhost UDP sockets with deterministic
  state verification.

### Multiplayer UI (2026-05-17)

The UI integration uses the existing state stack and menu system:

- **Menu items:** `HOST GAME` and `JOIN GAME` added to `MainMenu` enum and loaded in
  `Gfx::loadMenus()`.
- **Host flow:** Select HOST GAME → fades out → `Gfx::runOneFrame()` pushes
  `NetConnectState(Host)` which creates a `NetSession`, calls `hostGame(19532)`, and
  shows "HOSTING ON PORT 19532 / WAITING FOR PEER...". When the peer connects and
  handshake completes, `NetConnectState` releases the controller to `gfx->controller`
  and replaces itself with `GamePlayState` via `scheduleReplaceTop()`.
- **Join flow:** Select JOIN GAME → pushes `InputStringState` for the IP address →
  callback stores the address in `gfx->pendingNetAddress` and sets
  `gfx->pendingMenuSelection = MaJoinGame` → menu fades out → `runOneFrame()` pushes
  `NetConnectState(Client, address)` → connects to host → same flow as above.
- **Error handling:** Connection failures and disconnects are shown via `InfoBoxState`.
  During gameplay, `GamePlayState::update()` polls `gfx->netSession->update()` and
  detects disconnection, showing "PEER DISCONNECTED" if the peer drops.
- **Cleanup:** `gfx->netSession` is reset when returning to menu (game ended) or when
  starting a new local game (`MaNewGame`).

## Open Questions

- ~~What UDP library to use?~~ **Answered:** zpl-c/enet v2.6.5 via vcpkg overlay port. IPv6, reliable channels, lightweight.
- ~~Should we sync game settings/TC data hash to prevent mismatched clients from playing?~~ **Answered:** Yes. `NetSession::computeSettingsHash()` computes an FNV-1a hash of gameplay-affecting settings and both peers exchange it during handshake. Mismatch → Failed state.
- ~~Is there any state in `gvl::mwc` (the PRNG) beyond the two 32-bit values that needs synchronizing?~~ **Answered:** No, just `x` and `c`. Seed sync is sufficient.
- Should the input delay be adaptive (based on measured RTT) or fixed?
- How to handle disconnection more gracefully? Currently disconnects immediately; could add pause + timeout + reconnect.
- How should weapon selection work in multiplayer? Options: both players select locally then exchange, or use a shared selection screen with one player choosing at a time. Currently uses default weapons.
- Port configuration — currently hardcoded to 19532. Could add a port input field.

### Weapon selection in multiplayer (2026-05-17)

The weapon selection screen is now fully integrated with the lockstep network protocol.
Two issues had to be resolved:

1. **Weapon RNG desync:** The `WeaponSelection` constructor previously used `gfx.rand`
   (a local, non-synced RNG) for initial weapon randomization. Changed to `game.rand`
   (the synced game RNG) so both peers produce identical random weapons.

2. **`pressedOnce()` broken by lockstep `unpack()`:** In local play, `onKey()` only fires
   on actual key events, so `pressedOnce()` (which reads-and-clears a control bit) works
   correctly for one-shot navigation. In network play, `advanceWeaponSelection()` calls
   `unpack()` every frame, re-setting control bits that `pressedOnce()` had cleared — causing
   held keys to auto-repeat every frame. Fixed by implementing **edge detection**: tracking
   the previous frame's packed input state and only applying bits that transition from 0→1
   (rising edge). This matches the local behavior of receiving a key event only on press.

3. **Divergent wormSettings:** Each machine has different saved player profiles, so
   `wormSettings->weapons[]` differ between peers. Since the weapon selection constructor
   conditionally calls `game.rand()` based on whether `weapons[j] == 0`, different initial
   values cause different RNG call sequences → desync. Fixed by clearing all weapon
   preferences to 0 before constructing `WeaponSelection` in multiplayer, ensuring both
   peers take the identical randomization path.

## Future Work

- **Desync detection:** Periodic checksum comparison using the existing `PacketChecksum` packet type (already in the wire protocol, not yet wired up)
- **Replay recording of network games** — the data is already available (frame inputs)
- **Rollback netcode (Phase 2)** — GGPO-style prediction/rollback for internet play
- **NAT traversal** — STUN/TURN or relay server for connections through firewalls
- **Graceful disconnection** — pause + timeout + forfeit instead of immediate exit

### Settings and player info sync (2026-05-17)

Host-authoritative match settings model implemented:
- Both peers exchange `PacketPlayerInfo` (weapons[5] + color + rgb[3]) for their worm
- Host sends `PacketMatchSettings` with all gameplay settings; client applies them
- Game starts only when handshake + playerInfo + matchSettings all received
- Settings hash mismatch rejection removed (host is authority)

### In-game pressedOnce edge detection (2026-05-17)

The same `pressedOnce()` bug from weapon selection also affected in-game weapon scrolling
(holding Change+Left/Right cycled weapons every frame instead of once). The fix applies
the same rising-edge detection pattern in `advanceSimulation()`:

- Track `localPrevInput` / `remotePrevInput` per frame
- Rising edges (0→1): set bit via `|=`
- Released bits (1→0): clear bit via `&= ~`
- Held bits (1→1): leave unchanged — preserves `pressedOnce()` consumed state
- Both prev states reset to 0 on state transitions (weapon selection → game)

**Key discovery:** Worm respawn requires `pressedOnce(Fire)` to set `ready=true`, then
`doRespawning()` checks `ready` before making worm visible. With edge detection, this
works correctly — Fire on rising edge triggers once, `ready` persists across frames.

### Input ownership separation (2026-05-17)

Two additional bugs caused weapon switch desync and rapid-fire:

1. **SDL key repeats** — SDL3 fires `SDL_EVENT_KEY_DOWN` repeatedly while a key is held.
   These repeat events were calling `onKey()`, which re-set bits in `controlStates` that
   `pressedOnce()` had consumed. Fix: filter `ev.key.repeat` in `processEvent()`.

2. **onKey() bypassing edge detection** — `NetworkController::onKey()` directly modified
   `worm->controlStates`, but the remote peer only sees edge-detected network inputs.
   This caused the local peer to process different game state than the remote peer → desync.
   Fix: `onKey()` now only updates `localControlState` (the network packing source).
   Edge detection in `advanceSimulation()` is the sole writer of `worm->controlStates`.

**Design principle:** In network play, `worm->controlStates` must be modified identically
on both peers. Since both peers run the same edge detection on the same packed input bytes,
only the edge detection code should write to `controlStates`. Any other writer (like `onKey`)
creates a local-only modification invisible to the remote peer.

## Technical Risk Assessment

| Risk | Likelihood | Impact | Mitigation | Status |
|------|-----------|--------|------------|--------|
| Hidden non-determinism | ~~Medium~~ **Low** | Critical (desync) | Determinism test harness running in CI | ✅ Validated — 1000 frames, no desync |
| Input delay feels bad at >80ms RTT | High | Medium (UX) | Document as "alpha limitation", plan rollback for Phase 2 | Pending |
| NAT/firewall blocks connections | High | High (unusable) | Alpha = direct IP only; document port forwarding; Phase 2 adds hole-punching | Pending |
| Platform-specific fixed-point behavior | Low | Critical | Fixed-point is integer-based, should be identical; verify in harness | Pending (cross-platform CI) |
