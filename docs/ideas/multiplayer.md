# Multiplayer for Open Liero

> **⚠️ Keep this document up to date.** When multiplayer-related code changes, update the relevant sections here. This is the single source of truth for multiplayer architecture decisions, protocol details, and learnings.

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
- [x] **UDP hole-punching is feasible for later** — Replaced with full ICE (libjuice + coturn). Handles all NAT types including symmetric NATs via TURN relay. See "Online connect flow" section below.

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
7. ~~**Map transfer**~~ ✅ — Host generates the level and sends compressed map data to client during handshake, ensuring identical levels regardless of local files
8. ~~**TC + full settings sync**~~ ✅ — Two-phase TC exchange (hash check → archive transfer if needed), plus all gameplay-affecting settings synced from host to client

## How to Play

### LAN / Direct IP

1. Both players configure match settings (Match Setup) before connecting
2. Player A: select **HOST LAN GAME** from the main menu — shows "HOSTING ON PORT 19532"
3. Player B: select **JOIN LAN GAME**, type the host's IP address, press Enter
4. Both see connection status, then the game starts automatically
5. Press Escape during gameplay to disconnect and return to menu

### Online (NAT traversal)

1. Player A: select **HOST ONLINE** — discovers external IP via STUN, creates a room on the signaling server, and displays a 6-character room code
2. Player B: select **JOIN ONLINE**, type the room code, press Enter
3. Both peers perform STUN to learn their external address+port, then report addresses to the signaling server
4. The server signals both peers to begin UDP hole-punching simultaneously
5. If hole-punch succeeds → direct peer-to-peer connection (same as LAN)
6. If hole-punch fails → server allocates a UDP relay port and both peers connect through it (higher latency)
7. Press Escape at any point to cancel and return to menu

## Not Doing (and Why)

- **Rollback/prediction** — Adds massive complexity; lockstep with delay is sufficient for alpha and LAN play
- **More than 2 players** — The original game is 2-player; network protocol can be designed to allow expansion later but not implementing now
- ~~**Matchmaking server**~~ — Signaling server implemented (Go, `server/`); provides room codes, NAT traversal coordination, and relay fallback
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
  (9 bytes: type + seed + settingsHash), `PacketChecksum` (9 bytes: type + frame + checksum).
  Additional types: `PacketPlayerInfo` (4), `PacketMatchSettings` (5), `PacketMapData` (6),
  `PacketPause` (7), `PacketResume` (8), `PacketRematchReady` (9), `PacketRematchLevel` (10),
  `PacketEndMatch` (11), `PacketTcInfo` (12), `PacketTcResponse` (13), `PacketTcData` (14).
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
  game mode, blood, max bonuses, time to lose, flags to win, load change, regenerate level,
  shadow, names on bonuses, blood particle max, zone timeout).
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

### Multiplayer UI (2026-05-17, updated 2026-05-20)

The UI integration uses the existing state stack and menu system:

- **Menu items:** `HOST LAN GAME`, `JOIN LAN GAME`, `HOST ONLINE`, `JOIN ONLINE` added to
  `MainMenu` enum and loaded in `Gfx::loadMenus()`.
- **LAN host flow:** Select HOST LAN GAME → fades out → `Gfx::runOneFrame()` pushes
  `NetConnectState(Host)` which creates a `NetSession`, calls `hostGame(19532)`, and
  shows "HOSTING ON PORT 19532 / WAITING FOR PEER...". When the peer connects and
  handshake completes, `NetConnectState` releases the controller to `gfx->controller`
  and replaces itself with `GamePlayState` via `scheduleReplaceTop()`.
- **LAN join flow:** Select JOIN LAN GAME → pushes `InputStringState` for the IP address →
  callback stores the address in `gfx->pendingNetAddress` and sets
  `gfx->pendingMenuSelection = MaJoinGame` → menu fades out → `runOneFrame()` pushes
  `NetConnectState(Client, address)` → connects to host → same flow as above.
- **Online host flow:** Select HOST ONLINE → pushes `OnlineConnectState(Host)` →
  STUN → signaling → hole-punch → transitions to `NetConnectState` (see Online Connect Flow).
- **Online join flow:** Select JOIN ONLINE → input room code → pushes
  `OnlineConnectState(Client, code)` → same flow as host but joins existing room.
- **Error handling:** Connection failures and disconnects are shown via `InfoBoxState`.
  During gameplay, `GamePlayState::update()` polls `gfx->netSession->update()` and
  detects disconnection, showing "PEER DISCONNECTED" if the peer drops.
- **Cleanup:** `gfx->netSession` is reset when returning to menu (game ended) or when
  starting a new local game (`MaNewGame`).

## Open Questions

- ~~What UDP library to use?~~ **Answered:** zpl-c/enet v2.6.5 via vcpkg overlay port. IPv6, reliable channels, lightweight.
- ~~Should we sync game settings/TC data hash to prevent mismatched clients from playing?~~ **Answered:** Yes. Full TC data is transferred if hashes differ (two-phase: name+hash → need/skip → archive). All gameplay settings synced via `MatchSettingsData`. Settings hash includes all synced fields.
- ~~Is there any state in `gvl::mwc` (the PRNG) beyond the two 32-bit values that needs synchronizing?~~ **Answered:** No, just `x` and `c`. Seed sync is sufficient.
- Should the input delay be adaptive (based on measured RTT) or fixed?
- How to handle disconnection more gracefully? Currently disconnects immediately; could add pause + timeout + reconnect.
- ~~How should weapon selection work in multiplayer?~~ **Answered:** Both players select locally in lockstep; inputs exchanged via the same frame-synced protocol.
- ~~Port configuration — currently hardcoded to 19532. Could add a port input field.~~ Low priority: works as default; signaling server makes port forwarding unnecessary for most users.

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

- ~~**Desync detection:** Periodic checksum comparison using the existing `PacketChecksum` packet type (already in the wire protocol, not yet wired up)~~ ✅ Wired up — `fastGameChecksum()` sent every frame, compared on receipt
- **Replay recording of network games** — the data is already available (frame inputs)
- **Rollback netcode (Phase 2)** — GGPO-style prediction/rollback for internet play
- ~~**NAT traversal** — STUN/TURN or relay server for connections through firewalls~~ ✅ Implemented: STUN + signaling + hole-punch + relay fallback (see Online Connect Flow below)
- **Graceful disconnection** — pause + timeout + forfeit instead of immediate exit

### Map transfer (2026-05-17)

The host generates the level and sends compressed map data to the client during handshake.
This ensures both peers have byte-identical level data regardless of what level files are
available locally.

- **New packet type:** `PacketMapData` (type 6) carries compressed level data over reliable channel
- **Serialization format:** `compressed_flag(1) + uncompressed_size(4) + payload` where payload
  is zlib-compressed `width(2) + height(2) + rand_x(4) + rand_c(4) + pixels(w*h) + palette(768)`
- **RNG sync:** The post-generation RNG state (x, c) is included in the map data so the client's
  RNG matches the host's after level generation (which consumes variable RNG calls depending on
  level type)
- **Compression:** Uses miniz (already a vcpkg dependency). A typical 504×350 level (~177KB raw)
  compresses to ~10-50KB depending on content
- **Flow:** `tryStartGame()` → host calls `generateAndSendMap()` → client receives via
  `onMapData()` callback → `loadLevelFromData()` restores level + RNG state →
  `NetworkController::focus()` skips `generateFromSettings()` since `levelPreloaded` is set

### TC (total conversion) sync (2026-05-18)

The TC defines weapons, objects, sprites, sounds, materials, and constants. Both peers must
use identical TC data for deterministic simulation. A two-phase protocol avoids transferring
data when both players already have the same TC:

- **New packet types:** `PacketTcInfo` (type 12: hash + name), `PacketTcResponse` (type 13:
  needData flag), `PacketTcData` (type 14: compressed archive)
- **Protocol flow:**
  1. Host sends `PacketTcInfo` (FNV-1a hash of all TC file contents + TC name) on connect
  2. Client compares name and hash with local TC
  3. If match → client replies `PacketTcResponse(needData=false)` → TC resolved
  4. If mismatch → client replies `PacketTcResponse(needData=true)` → host packs and sends archive
- **Archive format:** `compressed_flag(1) + rawSize(4) + payload`. Raw payload is:
  `numFiles(4)` then per file `nameLen(2) + name + dataLen(4) + data`. Compressed with miniz.
- **TC hashing:** Files are collected recursively and sorted lexicographically by relative path.
  FNV-1a is computed over all filenames and file contents in order. This ensures deterministic
  hash regardless of filesystem enumeration order.
- **Client reload:** On receiving TC data, the client extracts files to `/tmp/openliero_tc_<name>`,
  creates a new `Common`, and fires `onTcReloaded` callback so `NetConnectState` can update
  `gfx.common` and reload the palette.
- **Implementation:** `src/game/net/tcArchive.hpp/cpp` provides `computeHash()`, `pack()`, and
  `unpack()` utilities.
- **Game start gating:** `tryStartGame()` requires `tcResolved_ == true` in addition to all
  other handshake flags.

### Settings and player info sync (2026-05-17, updated 2026-05-18)

Host-authoritative match settings model implemented:
- Both peers exchange `PacketPlayerInfo` (weapons[5] + color + rgb[3]) for their worm
- Host sends `PacketMatchSettings` with all gameplay settings; client applies them
- Game starts only when handshake + playerInfo + matchSettings + TC sync all received
- Settings hash mismatch rejection removed (host is authority)

**Full list of synced settings in `MatchSettingsData`:**
- `lives`, `loadingTime`, `gameMode`, `blood`, `maxBonuses`, `timeToLose`, `flagsToWin`
- `loadChange`, `weapTable[40]`
- `regenerateLevel` — whether level regenerates between rounds
- `shadow` — worm shadow rendering
- `namesOnBonuses` — bonus label display
- `bloodParticleMax` — maximum blood particles
- `zoneTimeout` — Holdazone capture/decay timing

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

### Uninitialized Bonus::weapon desync (2026-05-17)

The object pool (`ExactObjectList`) reuses slots without reinitializing them. When
`createBonus()` creates a health bonus (`frame==1`), it never set the `weapon` field —
leaving stale data from a previous pool occupant. While `weapon` is only read for weapon
bonuses (`frame==0`) in current game logic, this caused hash-based desync detection to
report false divergence between two identically-running game instances (different memory
layouts = different garbage).

**Fix:** Initialize `bonus->weapon = 0` in `createBonus()` unconditionally, and zero-init
all fields in the `Bonus` constructor.

**Lesson:** Any field that's included in state hashing must be deterministically
initialized, even if it's logically irrelevant for some object subtypes. Object pool
reuse means constructors only run once — all fields must be set at allocation time.

### Runtime desync detection (2026-05-17)

Wired up the existing `PacketChecksum` transport to actually send/compare checksums:
- `advanceSimulation()` computes `fastGameChecksum()` after every `processFrame()`
- Sent over unreliable channel (losing one is fine — next frame catches it)
- `NetSession::onChecksum()` compares local vs remote, sets `desyncDetected_` flag
- `GamePlayState` shows "DESYNC AT FRAME N" message and logs to stderr

### STUN external IP discovery (2026-05-19)

When hosting a game, the "CONNECT USING:" screen now shows the host's external (public) IP
addresses alongside local addresses, discovered via STUN (RFC 5389).

**Implementation** (`src/game/net/stun.hpp`, `src/game/net/stun.cpp`):
- Minimal STUN Binding Request using enet's socket API (`enet_socket_create`, `enet_socket_send`, `enet_socket_receive`)
- Two sequential queries in a background thread: one to Google's IPv4 STUN server (`74.125.250.129:19302`), one to their IPv6 server (`2001:4860:4864:5:8000::1:19302`)
- Parses `XOR-MAPPED-ADDRESS` (with `MAPPED-ADDRESS` fallback) from the response
- 2-second timeout, 2 retries per server — gracefully degrades (shows only local IPs if STUN fails)
- Results displayed with `(EXTERNAL)` label in the host waiting screen

**Design decisions:**
- Uses enet's raw socket API rather than adding a new dependency (libcurl, etc.)
- Direct IP literals for the STUN servers avoid DNS resolution (one fewer failure mode)
- Background thread avoids blocking the UI — external IPs appear asynchronously
- Extracts both external IP and port from XOR-MAPPED-ADDRESS
- STUN query binds to the game's local port (19532) for the LAN host screen display

### Online connect flow (ICE-based, 2026-05-21)

The online multiplayer flow is coordinated by `OnlineConnectState` (`src/game/onlineConnectState.hpp/cpp`),
using libjuice (RFC 8445 ICE) for NAT traversal and a signaling server for rendezvous.

**Components:**
- `IceAgent` (`src/game/net/iceAgent.hpp/cpp`) — libjuice C++ wrapper with thread-safe event queue
- `IceBridge` (`src/game/net/iceBridge.hpp/cpp`) — loopback UDP proxy between ENet and libjuice
- `SignalingClient` (`src/game/net/signaling.hpp/cpp`) — UDP-based signaling for room management and ICE credential/candidate exchange
- `StunQuery` — still used for LAN host screen to show external IP
- Go signaling server (`server/`) — coordinates rooms, forwards ICE messages, generates TURN credentials

**Flow (both host and client):**
1. `SignalingClient::createRoom()` or `joinRoom()` → server returns room code + optional TURN credentials
2. Create `IceAgent` with STUN/TURN servers configured
3. Start gathering → libjuice discovers host, server-reflexive, and relay candidates
4. Exchange ICE ufrag/pwd via signaling (`IceCredentials` ↔ `PeerCredentials`)
5. Exchange candidates as they're gathered (`IceCandidate` ↔ `PeerCandidate`)
6. Signal gather complete (`IceGatherDone` ↔ `PeerGatherDone`)
7. libjuice performs connectivity checks across all candidate pairs
8. On ICE Connected → create `IceBridge` (loopback UDP socket pair)
9. Create `NetTransport` on bridge socket, transition to `NetConnectState`

**TURN relay:** When direct connectivity fails (symmetric NATs, firewalls), libjuice
automatically uses the TURN relay candidate. No separate fallback path needed — TURN
is a first-class ICE candidate with appropriate priority.

**TURN credential generation:** The signaling server generates time-limited TURN credentials
using HMAC-SHA1 shared secret (standard TURN REST API pattern). Credentials expire after
24 hours. Requires `TURN_SECRET` env var matching the coturn server's `use-auth-secret`.

**Protocol** (binary UDP, must match `server/protocol.go`):
- Client→Server: `CreateRoom(0x01)`, `JoinRoom(0x02)`, `Keepalive(0x06)`, `IceCredentials(0x07)`, `IceCandidate(0x08)`, `IceGatherDone(0x09)`
- Server→Client: `RoomCreated(0x81)`, `PeerJoined(0x82)`, `RoomExpired(0x86)`, `PeerCredentials(0x87)`, `PeerCandidate(0x88)`, `PeerGatherDone(0x89)`, `Error(0x8F)`
- Legacy (ignored): `ReportAddr(0x03)`, `PunchOK(0x04)`, `PunchFail(0x05)`

**Key design decisions:**
- libjuice handles all NAT traversal (host, srflx, relay candidates + connectivity checks)
- IceBridge provides ENet a localhost socket, `cb_recv` writes from libjuice's thread safely
- ENet socket replacement: after `enet_host_create(nullptr, ...)`, swap auto-created socket with bridge socket
- Signaling uses enet's raw UDP socket API (not an ENet host) to avoid protocol interference
- Default signaling server: `liero-server.orbmit.org:19533`
- STUN server: Google's public STUN (`stun.l.google.com:19302`)

### Signaling server known limitations

**No authentication:** Any UDP sender can create rooms or join rooms.
For game matchmaking this is acceptable. Future mitigation: add HMAC-based auth if abuse becomes an issue.

**Room code entropy:** 6 chars from a 32-char alphabet = ~30 bits of entropy. Brute-forceable
at ~1M guesses/sec in ~17 minutes. Currently acceptable because rooms are short-lived (60s TTL)
and there's no incentive to join a stranger's game. Future mitigation: rate limiting per
source IP, or longer codes.

## Technical Risk Assessment

| Risk | Likelihood | Impact | Mitigation | Status |
|------|-----------|--------|------------|--------|
| Hidden non-determinism | ~~Medium~~ **Low** | Critical (desync) | Determinism + death fuzz test (5000+ frames per seed, 5 seeds) | ✅ Validated with fix |
| Input delay feels bad at >80ms RTT | High | Medium (UX) | Document as "alpha limitation", plan rollback for Phase 2 | Pending |
| NAT/firewall blocks connections | ~~High~~ **Low** | High (unusable) | ICE (libjuice) with TURN relay via coturn — handles all NAT types | ✅ Implemented |
| Platform-specific fixed-point behavior | Low | Critical | Fixed-point is integer-based, should be identical; verify in harness | Pending (cross-platform CI) |
