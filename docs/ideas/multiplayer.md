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

1. **Determinism harness** — Run two `Game` instances in-process with same seed/inputs, assert state matches after every frame
2. **NetworkController** — Implements `Controller` interface; owns UDP socket; buffers local+remote inputs; advances sim when both available
3. **Input delay buffer** — Ring buffer holding N frames of inputs; sim runs N frames behind real-time
4. **Protocol** — Frame number + input byte + periodic CRC32 checksum of game state
5. **Connection flow** — Host listens on port, peer connects, exchange game settings + seed, start game
6. **UI** — Menu option to host/join, IP entry field

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

## Open Questions

- What UDP library to use? Options: raw BSD sockets, SDL_net, ENet (reliable UDP with channels), or GameNetworkingSockets (Valve's library, has NAT traversal)
- Should the input delay be adaptive (based on measured RTT) or fixed?
- How to handle disconnection gracefully? Pause + timeout + forfeit?
- Should we sync game settings/TC data hash to prevent mismatched clients from playing?
- Is there any state in `gvl::mwc` (the PRNG) beyond the two 32-bit values that needs synchronizing?

## Technical Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Hidden non-determinism | Medium | Critical (desync) | Determinism test harness running in CI |
| Input delay feels bad at >80ms RTT | High | Medium (UX) | Document as "alpha limitation", plan rollback for Phase 2 |
| NAT/firewall blocks connections | High | High (unusable) | Alpha = direct IP only; document port forwarding; Phase 2 adds hole-punching |
| Platform-specific fixed-point behavior | Low | Critical | Fixed-point is integer-based, should be identical; verify in harness |
