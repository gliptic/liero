# Plan: Replace Custom NAT Traversal with libjuice + coturn

## Status: IMPLEMENTED ✓

All phases complete. Branch: `stun-support`

### Commits
1. `5e7dd33` — Add libjuice vcpkg dependency
2. `245fe61` — Add IceAgent wrapper (~150 lines)
3. `73a31a8` — Add IceBridge loopback proxy (~120 lines)
4. `a255b9b` — Add ICE unit tests (8 test cases)
5. `e3fdf5f` — Add ICE signaling protocol messages
6. `18d6f86` — Rewrite OnlineConnectState for ICE flow
7. `5472122` — Rewrite Go signaling server for ICE
8. `cc45a62` — Remove dead punch/relay/StunViaHost code (-548 lines)

### Notes
- **StunQuery retained** — Still used by LAN host screen to show external IP
- **test_stun.cpp retained** — Tests RFC 5389 parsing utilities (still used)
- **Tests combined** — IceAgent + IceBridge tests in single `test_ice.cpp` (not split across 3 files as originally planned)
- **TURN deployment** — Requires coturn server with shared secret matching `TURN_SECRET` env var on signaling server
- **setRemoteGatheringDone() not called** — Omitted intentionally. UDP can reorder packets so gather-done arrives before a late candidate; libjuice then rejects it. Without the call, libjuice uses an internal timeout instead — same result, no race condition.
- **signaling.poll() drains all packets** — Changed from reading one packet per call to draining all available, reducing multi-packet latency

---

## Problem

The current online P2P implementation uses a custom STUN/hole-punch/relay stack that fails in many real-world NAT scenarios:

1. **No full ICE** — Only does blind hole-punching to STUN-observed addresses. Symmetric NATs remap ports per-destination, making the STUN-discovered port useless for the game peer.
2. **No connectivity checks** — Probes are fire-and-forget with a nonce match. There's no RFC-compliant candidate pair validation or priority-based fallback.
3. **No relay candidates in ICE** — The relay is a separate fallback path triggered only after both peers report failure, adding delay.
4. **Custom relay protocol** — The Go relay uses a bespoke token-auth scheme that's fragile and not reusable.

These limitations mean hole-punching works only on easy NATs (full-cone, address-restricted). Symmetric NATs (common on mobile carriers, corporate networks, and some ISPs) always fall through to relay.

## Solution

Replace the custom NAT traversal layer with **libjuice** (a lightweight RFC 8445 ICE library) and **coturn** (standard TURN relay server). This gives us:

- Full ICE with host + server-reflexive + relay candidates
- Proper connectivity checks with candidate pair prioritization
- TURN relay as a first-class candidate (no separate fallback path)
- Battle-tested code handling all NAT types
- ~150 lines of glue code (IceAgent + IceBridge) replacing ~1200 lines of custom C++ and Go

## What We Keep

- **ENet** — Still used for reliable/unreliable game transport on top of the established UDP path
- **NetTransport** — Keeps all game packet types, just loses punch/relay code
- **NetSession / NetworkController** — Unchanged game logic
- **Go signaling server** — Simplified to relay ICE credentials + candidates (room codes stay)
- **OnlineConnectState** — Same UI flow, just drives libjuice instead of custom punch

## Architecture (After)

```
┌──────────────────────────────────────────────────────┐
│                   OnlineConnectState                  │
│                                                      │
│  1. Create libjuice agent (with STUN+TURN config)    │
│  2. Signaling: create/join room                      │
│  3. Exchange ICE ufrag/pwd + candidates via signal   │
│  4. libjuice performs ICE (checks all candidate      │
│     pairs, including TURN relay)                     │
│  5. On success → create IceBridge (loopback proxy)   │
│  6. Create ENet host connected to bridge             │
│  7. Transition to NetConnectState (same as today)    │
└──────────────────────────────────────────────────────┘

     ENet ←UDP→ [127.0.0.1:A] ←→ [127.0.0.1:B] ←bridge→ juice_send/cb_recv ←→ Internet
```

### Why a Loopback Bridge?

libjuice does **not expose** its socket fd (the socket is private to `conn_impl`).
After ICE completes, the only way to send/receive application data is through
`juice_send()` and the `cb_recv` callback. For TURN relay connections, the TURN
allocation lives inside libjuice — destroying the agent would kill the allocation.

The loopback bridge creates two connected UDP sockets on localhost:
- Socket A: given to ENet as its transport socket
- Socket B: our bridge code reads from B → `juice_send()`, and `cb_recv` → writes to A

This adds <0.1ms of latency (loopback) and works uniformly for both direct and
relay connections. libjuice stays alive handling STUN/TURN keepalives.

### Threading Model

libjuice callbacks (`cb_state_changed`, `cb_candidate`, `cb_gathering_done`,
`cb_recv`) all fire from an **internal background thread** (no user-facing poll API).

Solution: A thread-safe event queue in `IceAgent`:
- Callbacks push events (state changes, candidates, received data) onto the queue
- Main game thread calls `IceAgent::poll()` each frame to drain the queue
- `cb_recv` is special: it writes directly to the bridge socket (UDP writes are atomic)

libjuice's public API (`juice_send`, `juice_add_remote_candidate`, etc.) is thread-safe
internally (uses recursive mutex), so calling them from the main thread is fine.

## Dependencies

| Dependency | Source | License | Purpose |
|---|---|---|---|
| libjuice | vcpkg (`libjuice`) | BSD-2-Clause | ICE agent (STUN + TURN + connectivity checks) |
| coturn | Docker / apt | Custom permissive | TURN relay server (replaces Go relay) |

libjuice is ~3000 lines of pure C with zero external dependencies. It supports:
- RFC 8445 (ICE)
- RFC 5389 (STUN)
- RFC 5766/8656 (TURN)
- UDP only (perfect for our use case)

## Implementation Phases

### Phase 1: Add libjuice and Create IceAgent + IceBridge

**Files to create:**
- `src/game/net/iceAgent.hpp` — C++ wrapper around libjuice
- `src/game/net/iceAgent.cpp` — Implementation
- `src/game/net/iceBridge.hpp` — Loopback UDP bridge between libjuice and ENet
- `src/game/net/iceBridge.cpp` — Implementation

**IceAgent API:**

```cpp
struct IceAgent {
  struct Config {
    std::string stunServer = "stun.l.google.com";
    uint16_t stunPort = 19302;
    std::string turnServer;   // e.g., "turn.liero-server.orbmit.org"
    uint16_t turnPort = 3478;
    std::string turnUser;
    std::string turnPassword;
    // Uses JUICE_CONCURRENCY_MODE_THREAD (default) — required for TURN support
  };

  void start(const Config& config);
  void stop();

  // Get local ICE credentials (send to peer via signaling)
  std::string localUfrag() const;
  std::string localPwd() const;

  // Set remote ICE credentials (received from peer via signaling)
  void setRemoteCredentials(const std::string& ufrag, const std::string& pwd);

  // Add a remote candidate (received from peer via signaling)
  void addRemoteCandidate(const std::string& candidate);

  // Signal that all remote candidates have been provided
  void setRemoteGatheringDone();

  // State
  enum State { New, Gathering, Connecting, Connected, Failed, Disconnected };
  State state() const;

  // Poll for state changes (drains internal event queue, call from main thread)
  void poll();

  // Send application data through the ICE connection
  void send(const uint8_t* data, size_t len);

  // Callbacks (fired from poll() on main thread, NOT from libjuice's thread)
  std::function<void(State)> onStateChange;
  std::function<void(const std::string& candidate)> onLocalCandidate;
  std::function<void()> onGatheringDone;

private:
  juice_agent_t* agent_ = nullptr;
  // Thread-safe event queue: libjuice callbacks push, poll() drains
  std::mutex mutex_;
  std::vector<Event> pendingEvents_;
};
```

**IceBridge API:**

```cpp
// Creates a localhost UDP socket pair and bridges between ENet and IceAgent.
// ENet sends/receives on one socket; bridge proxies to/from juice_send/cb_recv.
struct IceBridge {
  // Create the bridge. Returns the ENet-side socket fd.
  // ENet should use this as its host->socket.
  ENetSocket create(IceAgent& agent);

  // Poll the bridge: reads outgoing ENet data from bridge socket → juice_send().
  // Call once per frame from the main loop (before or after enet_host_service).
  void poll();

  // Destroy both sockets.
  void destroy();

private:
  ENetSocket enetSocket_ = ENET_SOCKET_NULL;   // ENet's end (127.0.0.1:portA)
  ENetSocket bridgeSocket_ = ENET_SOCKET_NULL; // Our end (127.0.0.1:portB)
  IceAgent* agent_ = nullptr;
};
```

**How the bridge works:**
1. `create()` binds two UDP sockets to `127.0.0.1` with ephemeral ports, `connect()`s
   each to the other's address (so send/recv work without specifying addresses)
2. Both sockets configured: non-blocking, SO_RCVBUF=256KB, SO_SNDBUF=256KB (matching ENet defaults)
3. `enetSocket_` is given to ENet (replace `host->socket` after `enet_host_create`)
4. IceAgent's `cb_recv` callback writes received data to `enetSocket_` via
   `sendto(bridgeSocket_, data, len, 0, enetAddr)` — safe from any thread (separate socket objects)
5. `poll()` reads from `bridgeSocket_` (non-blocking) and calls `agent->send()` for each datagram
6. ENet sees a normal UDP socket and works unmodified

**Build system:**
- Add `"libjuice"` to `vcpkg.json`
- Add `find_package(LibJuice)` and link in `CMakeLists.txt`

### Phase 2: Update Signaling Protocol

The signaling server needs to relay ICE data instead of raw addresses. New protocol:

**New messages (replacing ReportAddr/StartPunch/PunchOK/PunchFail):**

```
Client → Server:
  IceCredentials [0x07] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
  IceCandidate   [0x08] + [6: room code] + [2: candidate_len BE] + [N: candidate SDP string]
  IceGatherDone  [0x09] + [6: room code]

Server → Client:
  PeerCredentials [0x87] + [6: room code] + [1: ufrag_len] + [N: ufrag] + [1: pwd_len] + [N: pwd]
  PeerCandidate   [0x88] + [6: room code] + [2: candidate_len BE] + [N: candidate SDP string]
  PeerGatherDone  [0x89] + [6: room code]
```

**TURN credentials in RoomCreated/PeerJoined:**

The server generates time-limited TURN credentials and includes them in existing responses:

```
RoomCreated [0x81] + [6: room code] + [1: turn_user_len] + [N: turn_user] + [1: turn_pass_len] + [N: turn_pass]
PeerJoined  [0x82] + [6: room code] + [1: turn_user_len] + [N: turn_user] + [1: turn_pass_len] + [N: turn_pass]
```

If no TURN server is configured, `turn_user_len` and `turn_pass_len` are both 0.
The TURN server hostname/port are compile-time constants in the client (same as the
signaling server address today). Only the per-session credentials are dynamic.

**Removed messages:** `ReportAddr (0x03)`, `PunchOK (0x04)`, `PunchFail (0x05)`, `StartPunch (0x84)`, `UseRelay (0x85)`

**Keep:** `CreateRoom`, `JoinRoom`, `RoomCreated`, `PeerJoined`, `Keepalive`, `RoomExpired`, `Error`

**Server changes:**
- Remove `Relay` struct and relay allocation logic entirely
- Room no longer tracks `Addresses []PeerAddr` or `PunchOK/PunchFail`
- Simply forwards ICE credentials and candidates between peers
- Server becomes ~150 lines (down from ~470)

### Phase 3: Update SignalingClient (C++)

**Modify:** `src/game/net/signaling.hpp` and `signaling.cpp`

- Remove: `reportAddress()`, `reportPunchOK()`, `reportPunchFail()`
- Remove: `onStartPunch`, `onUseRelay` callbacks
- Remove: `PeerCandidate` struct (replaced by raw SDP candidate strings)
- Add: `sendIceCredentials(ufrag, pwd)`
- Add: `sendIceCandidate(sdpCandidate)`
- Add: `sendIceGatherDone()`
- Add: `onPeerCredentials(ufrag, pwd)` callback
- Add: `onPeerCandidate(sdpCandidate)` callback
- Add: `onPeerGatherDone()` callback

### Phase 4: Update OnlineConnectState

**Modify:** `src/game/onlineConnectState.hpp` and `onlineConnectState.cpp`

New flow:

```
enter():
  1. Create IceAgent with STUN + TURN config
  2. Wire onLocalCandidate → buffer candidates until signaling ready
  3. Wire onStateChange → transition on Connected/Failed
  4. Start IceAgent (begins gathering)
  5. Connect to signaling server (create/join room)

onRoomCreated / onJoinAcked:
  → Send local ICE credentials to signaling
  → Send any buffered local candidates
  → Send gatherDone if gathering already finished

onPeerCredentials:
  → agent.setRemoteCredentials(ufrag, pwd)

onPeerCandidate:
  → agent.addRemoteCandidate(candidate)

onPeerGatherDone:
  → agent.setRemoteGatheringDone()

onLocalCandidate:
  → signaling.sendIceCandidate(candidate)

onGatheringDone:
  → signaling.sendIceGatherDone()

update() (each frame):
  → agent.poll()  // drains event queue, fires callbacks on main thread

onStateChange(Connected):
  → Create IceBridge, get ENet-side socket fd
  → Create ENet host with address=NULL (unbound socket)
  → Close ENet's auto-created socket, replace host->socket with bridge socket
  → enet_host_connect() to a dummy address (127.0.0.1:bridgePort)
  → Bridge handles all real I/O transparently
  → Transition to NetConnectState (same as today's connectDirect)

onStateChange(Failed):
  → Show "CONNECTION FAILED" error
```

**Remove:**
- `StunViaHost stunViaHost_`
- All punch-related state (`startedPunch_`, `punchRequested_`, etc.)
- `startPunching()`, `onPunchSuccess()`, `onPunchFailed()`, `connectRelay()`

### Phase 5: Clean Up NetTransport

**Modify:** `src/game/net/transport.hpp` and `transport.cpp`

- Remove: `startPunch()`, `stopPunch()`, `sendProbes()`, `punchPoll()`
- Remove: `PunchCandidate`, `PunchResult`, `PunchState` and all punch state fields
- Remove: `hostViaRelay()`, `connectViaRelay()`, `sendRelayToken()` and relay state fields
- Remove: `PROBE_MAGIC`, relay constants
- Remove: `onPunchSuccess`, `onPunchTimeout` callbacks
- Remove: `onInterceptedPacket` callback (STUN no longer goes through ENet)
- Keep: `host()`, `connect()`, `connectExisting()`, all game packet types

Add a new method for online mode:
```cpp
// Create ENet host using the bridge socket (from IceBridge).
// The bridge socket is already connected to the bridge's other end on localhost.
// ENet sees it as a normal UDP socket and works unmodified.
bool createHostOnBridgeSocket(ENetSocket bridgeSocket);
```

This creates the ENet host with `address=NULL`, then swaps `host->socket` with
the provided bridge socket (closing the auto-created one). ENet is unaware that
its "network" is actually a localhost proxy to libjuice.

### Phase 6: Tests for IceAgent + IceBridge

**Files to create:**
- `src/tests/test_ice_agent.cpp`
- `src/tests/test_ice_bridge.cpp`
- `src/tests/test_ice_integration.cpp`

**test_ice_agent.cpp — Unit tests for IceAgent wrapper:**

| Test | Description |
|------|-------------|
| Agent starts and gathers candidates | Create agent with STUN config, verify state transitions New→Gathering, verify at least one host candidate is emitted via `onLocalCandidate`, verify `onGatheringDone` fires |
| Local credentials are available after start | Call `localUfrag()` and `localPwd()` after `start()`, verify non-empty and reasonable length |
| Two local agents connect directly | Create two agents (no STUN/TURN, host candidates only on 127.0.0.1), exchange credentials and candidates manually, poll both, verify both reach `Connected` state |
| Remote credentials + candidates wiring | Set remote credentials and add remote candidate, verify no crash and state progresses toward Connecting |
| Agent stop is clean | Start agent, stop it, verify no callbacks fire after stop, verify double-stop is safe |
| Poll drains event queue correctly | Start agent, let callbacks accumulate, call `poll()`, verify all events delivered in order on calling thread |
| State transitions fire onStateChange | Wire `onStateChange`, drive agent through lifecycle, verify expected state sequence |
| Failed state on invalid TURN | Configure agent with unreachable TURN server, verify it reaches `Failed` (not hung) within timeout |

**test_ice_bridge.cpp — Unit tests for IceBridge loopback proxy:**

| Test | Description |
|------|-------------|
| Bridge creates valid socket pair | Call `create()`, verify returned ENet socket is valid, verify both internal sockets are bound to 127.0.0.1 |
| Data sent on ENet socket arrives at bridge poll | Write a datagram to the ENet-side socket, call `poll()`, verify `agent->send()` is called with the same data (use a mock/spy IceAgent) |
| Data from IceAgent cb_recv arrives on ENet socket | Simulate `cb_recv` by writing to the bridge socket from another thread, verify `recvfrom()` on the ENet socket returns the data |
| Large datagrams (MTU boundary) | Send 1200-byte and 1500-byte datagrams through bridge in both directions, verify no truncation or corruption |
| Multiple datagrams in one poll cycle | Write N datagrams to ENet socket before calling `poll()`, verify all N are forwarded via `agent->send()` |
| Bridge destroy closes both sockets | Call `destroy()`, verify both sockets are invalid afterwards, verify double-destroy is safe |
| Non-blocking behavior | Verify `poll()` returns immediately when no data is pending (doesn't block) |

**test_ice_integration.cpp — End-to-end ICE connection over localhost:**

| Test | Description |
|------|-------------|
| Two peers connect and exchange data via bridge | Full flow: two IceAgents + two IceBridges, exchange credentials/candidates, wait for Connected, send datagrams through bridges in both directions, verify arrival |
| ENet over ICE bridge | Two IceAgents + bridges, create ENet hosts on bridge sockets, `enet_host_connect`/accept, send reliable packets, verify delivery |
| Full NetTransport over ICE | Simulate the real flow: IceAgent connect → bridge → `createHostOnBridgeSocket()` → NetTransport handshake completes (same pattern as test_session.cpp but over ICE) |
| Connection survives sustained traffic | After ICE connect + bridge setup, send 1000 datagrams at 70/sec rate, verify all arrive (no silent drops on loopback) |
| ICE failure propagates to ENet | Start ICE connection, forcibly destroy one agent mid-session, verify the other side's ENet detects disconnection within timeout |
| Candidate ordering doesn't matter | Exchange candidates before credentials for one peer, verify connection still succeeds (tests buffering) |
| Gathering completes before peer joins | One agent fully gathers, then exchanges credentials. Verify trickle-after-gather works correctly |

**Test infrastructure notes:**
- Use Catch2 (same as existing tests)
- For IceBridge unit tests, create a minimal mock `IceAgent` that records `send()` calls and allows injecting `cb_recv` data
- Integration tests use real libjuice agents over localhost (no network dependency)
- All tests must complete within 5 seconds (ICE over localhost should connect in <500ms)

### Phase 7: Remove Old STUN Code

**Delete:**
- `src/game/net/stun.hpp`
- `src/game/net/stun.cpp`
- `src/tests/test_stun.cpp`

### Phase 8: Deploy coturn (Server-Side)

Replace the Go relay with a standard coturn deployment:

```yaml
# docker-compose.yml addition for the server
coturn:
  image: coturn/coturn
  network_mode: host
  volumes:
    - ./turnserver.conf:/etc/turnserver.conf
  command: ["-c", "/etc/turnserver.conf"]
```

```ini
# turnserver.conf
listening-port=3478
realm=liero-server.orbmit.org
use-auth-secret
static-auth-secret=<GENERATED_SECRET>
total-quota=100
max-bps=256000
no-multicast-peers
fingerprint
```

The signaling server can generate time-limited TURN credentials using the shared secret (standard TURN REST API pattern):

```go
func generateTurnCredentials(secret string) (user, pass string) {
    expiry := time.Now().Add(24 * time.Hour).Unix()
    user = fmt.Sprintf("%d", expiry)
    mac := hmac.New(sha1.New, []byte(secret))
    mac.Write([]byte(user))
    pass = base64.StdEncoding.EncodeToString(mac.Sum(nil))
    return
}
```

The signaling server sends TURN credentials to clients in the `RoomCreated`/`PeerJoined` response so they can configure their ICE agent.

### Phase 9: Update Signaling Server (Go)

**Simplify:** `server/server.go`, `server/protocol.go`

- Remove: `relay.go` entirely
- Remove: `Relay` field from `Room`, relay port allocation, relay goroutines
- Remove: `PeerAddr`, `Addresses`, `PunchOK/PunchFail` from `Peer`/`Room`
- Add: Handle `IceCredentials`, `IceCandidate`, `IceGatherDone` messages (just forward to other peer)
- Add: Include TURN credentials in `RoomCreated`/`PeerJoined` responses

The server becomes a pure message relay (~150 lines).

## Migration Strategy

1. **Feature-flag approach:** Add a compile-time flag `USE_LIBJUICE` so both paths can coexist during development
2. **LAN mode unaffected:** Direct IP connection (`HOST LAN`/`JOIN LAN`) doesn't use ICE at all — unchanged
3. **Test with symmetric NAT:** Use `iptables` to simulate symmetric NAT and verify TURN fallback works
4. **Backwards-incompatible:** The new signaling protocol is not compatible with the old one. Both client and server must be updated together. This is fine for an alpha.
5. **Development order:** Implement client (Phases 1-4) and server (Phase 8) in parallel since both sides are needed for integration testing. Use a local Go server for development. Phase 2-3 (signaling protocol) should be implemented on both sides simultaneously.
6. **Trickle ICE vs full gather:** If gathering completes before the peer joins the room, all candidates are sent at once (effectively non-trickle). This is valid ICE behavior and simpler — no need to delay gathering for the peer.

## Design Decisions (Investigated)

### ICE Role Assignment

libjuice has **no explicit role configuration API**. Roles are determined automatically by call order:

- The agent that calls `juice_gather_candidates()` before `juice_set_remote_description()` becomes **CONTROLLING** (offerer).
- The agent that calls `juice_set_remote_description()` first becomes **CONTROLLED** (answerer).

If both agents end up with the same role (e.g., race condition), libjuice implements full RFC 8445 §7.3.1.1 tiebreaker-based conflict resolution automatically using a random 64-bit tiebreaker value.

**For our implementation:** The host (room creator) should call `juice_gather_candidates()` immediately on agent creation. The joiner should call `juice_set_remote_description()` with the host's credentials before calling `juice_gather_candidates()`. This naturally produces host=CONTROLLING, joiner=CONTROLLED. Even if both gather first (since we start gathering before signaling completes), the tiebreaker resolves it.

### TURN Allocation Refresh

libjuice **automatically refreshes TURN allocations**. Internally:

- `TURN_LIFETIME = 600000ms` (10 minutes)
- `TURN_REFRESH_PERIOD = 540000ms` (9 minutes — 1 minute before expiry)

The bookkeeping loop sends `STUN_METHOD_REFRESH` requests automatically on the nominated relay candidate. TURN permissions and channel bindings are also refreshed internally. Applications do not need to manage any keepalives.

If the connection uses a direct path (not relayed), libjuice stops refreshing the unused TURN allocation and lets it expire naturally.

**Note:** TURN is only supported in `JUICE_CONCURRENCY_MODE_THREAD` (default) or `JUICE_CONCURRENCY_MODE_POLL`, **not** `JUICE_CONCURRENCY_MODE_MUX`. We'll use the default thread mode.

### ENet Socket Replacement Safety

Verified by inspecting ENet 2.6.5 source (`enet.h:4542-4585`). After `enet_host_create()`:

- ENet only stores `host->socket` and `host->address` as socket-specific state
- `enet_host_service()` uses `host->socket` at call time — no cached fd elsewhere
- `enet_host_create()` applies socket options (NONBLOCK, BROADCAST, RCVBUF/SNDBUF, IPV6_V6ONLY) to the original socket

**Safe to replace before first `enet_host_service()` call**, provided the bridge socket is pre-configured with:
- Non-blocking mode
- Adequate SO_RCVBUF/SO_SNDBUF (ENet sets 256KB each)

We do NOT need BROADCAST or IPV6_V6ONLY on a localhost socket. The bridge's `create()` method should set NONBLOCK and buffer sizes to match ENet's expectations.

### Bridge Thread Safety (`cb_recv` → `sendto` from libjuice thread)

**Confirmed safe on all platforms.** The bridge uses two separate sockets:
- Socket B (bridge side): libjuice's `cb_recv` calls `sendto(socketB, ...)` from its internal thread
- Socket A (ENet side): main thread calls `recvfrom(socketA, ...)` via `enet_host_service()`

Since these are different kernel objects, there is zero lock contention:
- **Linux:** Send path is lockless (fast path). Receive uses per-queue spinlock on socketA. No shared locks.
- **macOS/BSD:** Each socket has its own `so_lock` mutex. Different sockets = different mutexes.
- **Windows:** Winsock/AFD handles concurrent I/O on different handles independently.

Loopback delivery between the two sockets is handled by kernel-internal synchronization (spinlocks on the receive queue). No user-space synchronization is needed.

**Edge case:** If socketA's `SO_RCVBUF` fills up, datagrams are silently dropped (standard UDP behavior). Mitigated by setting adequate buffer sizes and the fact that loopback delivery is near-instant.

## Risk Assessment

| Risk | Mitigation |
|---|---|
| libjuice doesn't expose socket fd | Loopback bridge pattern: ENet talks to localhost, bridge proxies to/from `juice_send()`/`cb_recv()`. Adds <0.1ms latency. |
| libjuice callbacks fire from internal thread | Thread-safe event queue for state/candidate events. `cb_recv` writes directly to bridge socket (atomic UDP writes, safe cross-platform — see above). Main thread polls queue each frame. |
| ENet doesn't natively support custom transport | Replace `host->socket` after creation with bridge socket. ENet uses `sendto()`/`recvfrom()` on it unmodified. Bridge socket must be pre-configured NONBLOCK + adequate buffer sizes. |
| TURN adds latency | Expected ~20-50ms extra RTT vs direct. Acceptable for lockstep with 3-frame delay. Better than no connection at all. |
| coturn operational complexity | Docker one-liner. Can also use free public TURN servers (e.g., Metered.ca free tier) for testing. |
| libjuice doesn't support IPv6 TURN | libjuice supports IPv6 for host/srflx candidates. TURN over IPv4 is sufficient as fallback. |
| Bridge adds complexity vs direct socket | ~40 lines of code total. Uniform path for all connection types (no special-casing direct vs relay). |
| ICE role conflict | Handled automatically by libjuice via RFC 8445 tiebreaker. No application logic needed. |
| TURN allocation expires mid-game | libjuice refreshes automatically every 9 minutes. No application logic needed. |

## Files Changed Summary

| File | Action |
|---|---|
| `vcpkg.json` | Add `libjuice` dependency |
| `CMakeLists.txt` | Add libjuice find/link |
| `src/game/net/iceAgent.hpp` | **Create** — libjuice wrapper with thread-safe event queue |
| `src/game/net/iceAgent.cpp` | **Create** — implementation |
| `src/game/net/iceBridge.hpp` | **Create** — loopback UDP bridge between libjuice and ENet |
| `src/game/net/iceBridge.cpp` | **Create** — implementation |
| `src/game/net/signaling.hpp` | Modify — new ICE message types |
| `src/game/net/signaling.cpp` | Modify — new ICE message types |
| `src/game/net/transport.hpp` | Modify — remove punch/relay, add `createHostOnBridgeSocket()` |
| `src/game/net/transport.cpp` | Modify — remove punch/relay code |
| `src/game/net/stun.hpp` | **Delete** |
| `src/game/net/stun.cpp` | **Delete** |
| `src/game/onlineConnectState.hpp` | Modify — use IceAgent + IceBridge |
| `src/game/onlineConnectState.cpp` | Modify — new ICE-based flow |
| `src/tests/test_stun.cpp` | **Delete** |
| `src/tests/test_ice_agent.cpp` | **Create** — IceAgent unit tests |
| `src/tests/test_ice_bridge.cpp` | **Create** — IceBridge loopback proxy tests |
| `src/tests/test_ice_integration.cpp` | **Create** — End-to-end ICE + ENet integration tests |
| `server/relay.go` | **Delete** |
| `server/server.go` | Simplify — remove relay, add ICE forwarding + TURN cred generation |
| `server/protocol.go` | Update message types |
| `server/server_test.go` | Update tests |

## Estimated Effort

- Phase 1 (IceAgent wrapper): ~2 hours
- Phase 2-3 (Signaling protocol): ~2 hours
- Phase 4 (OnlineConnectState): ~2 hours
- Phase 5 (Transport cleanup): ~1 hour
- Phase 6 (Tests): ~3 hours
- Phase 7 (Remove old STUN): ~15 minutes
- Phase 8-9 (Server): ~2 hours
- Integration testing & debugging: ~2 hours

**Total: ~14 hours** (vs maintaining/debugging the current custom stack indefinitely)

## Success Criteria

1. Two peers behind different NAT types can connect and play
2. Symmetric NAT → TURN relay used automatically (no manual fallback)
3. Easy NAT (full-cone) → direct P2P connection established
4. LAN play continues to work unchanged
5. Connection time ≤ 5 seconds on easy NATs, ≤ 10 seconds via TURN
6. No regression in gameplay determinism or desync detection
