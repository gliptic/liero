// Desync fuzzer: stress-tests lockstep determinism with randomized inputs and settings.
// Runs two NetworkControllers in loopback and compares full game state every frame.
//
// Usage: desync_fuzzer [--iterations N] [--frames N] [--seed N] [--jobs N]

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "controller/networkController.hpp"
#include "game.hpp"
#include "math.hpp"
#include "mixer/player.hpp"
#include "settings.hpp"
#include "stateHash.hpp"

struct FuzzerConfig {
  int iterations = 100;
  int frames = 30000;
  int jobs = 16;
  int jitter = 0;  // max extra delivery delay in ticks (0 = instant)
  uint32_t seed = 0;  // 0 = time-based
};

static FuzzerConfig parseArgs(int argc, char* argv[]) {
  FuzzerConfig cfg;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
      cfg.iterations = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      cfg.frames = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
      cfg.seed = static_cast<uint32_t>(atol(argv[++i]));
    } else if ((strcmp(argv[i], "--jobs") == 0 || strcmp(argv[i], "-j") == 0) && i + 1 < argc) {
      cfg.jobs = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--jitter") == 0 && i + 1 < argc) {
      cfg.jitter = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printf("Usage: desync_fuzzer [--iterations N] [--frames N] [--seed N] [--jobs N] [--jitter N]\n");
      printf("  --iterations N  Number of iterations (default: 100)\n");
      printf("  --frames N      Frames per iteration (default: 30000)\n");
      printf("  --seed N        Fixed seed (default: time-based)\n");
      printf("  --jobs N, -j N  Parallel workers (default: 16)\n");
      printf("  --jitter N      Max extra delivery delay in ticks (default: 0 = instant)\n");
      exit(0);
    }
  }
  return cfg;
}

// Simple xorshift RNG for fuzzer decisions (separate from game RNG)
struct FuzzerRng {
  uint32_t state;
  FuzzerRng(uint32_t s) : state(s ? s : 1) {}
  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
  uint32_t range(uint32_t max) { return next() % max; }
};

static void randomizeSettings(FuzzerRng& rng, Settings& settings) {
  settings.lives = 1 + rng.range(20);
  settings.blood = rng.range(500);
  settings.loadingTime = rng.range(100);
  settings.loadChange = rng.range(2) != 0;
  settings.randomLevel = true;
  settings.gameMode = rng.range(Settings::MaxGameModes);
  settings.maxBonuses = rng.range(10);
  settings.timeToLose = 100 + rng.range(500);
  settings.flagsToWin = 1 + rng.range(5);
  settings.shadow = rng.range(2) != 0;
  settings.namesOnBonuses = rng.range(2) != 0;
  settings.regenerateLevel = rng.range(2) != 0;

  // Randomize weapon table (enable/disable weapons)
  for (int i = 0; i < 40; ++i) {
    settings.weapTable[i] = rng.range(3);  // 0=disabled, 1=enabled, 2=bonus-only
  }
}

// Generate combat-biased random input for one frame.
// Fire is held ~60% of the time, movement biased, occasional weapon switch.
static uint8_t randomInput(FuzzerRng& rng) {
  uint8_t state = 0;
  // Bit layout matches WormControlState::pack():
  // 0=Up, 1=Down, 2=Left, 3=Right, 4=Fire, 5=Change, 6=Jump
  if (rng.range(100) < 60) state |= (1 << 4);  // Fire
  if (rng.range(100) < 30) state |= (1 << 0);  // Up (aim)
  if (rng.range(100) < 30) state |= (1 << 1);  // Down (aim)
  if (rng.range(100) < 40) state |= (1 << 2);  // Left
  if (rng.range(100) < 40) state |= (1 << 3);  // Right
  if (rng.range(100) < 5)  state |= (1 << 5);  // Change weapon
  if (rng.range(100) < 15) state |= (1 << 6);  // Jump
  return state;
}

struct InputRecord {
  uint32_t frame;
  uint8_t inputA;
  uint8_t inputB;
};

struct DesyncInfo {
  uint32_t seed;
  int frame;
  uint32_t hashA;
  uint32_t hashB;
  ComponentHashes componentsA;
  ComponentHashes componentsB;
  std::vector<InputRecord> recentInputs;
};

static std::mutex outputMutex;

static void printDesync(const DesyncInfo& info, const Settings& settings) {
  std::lock_guard<std::mutex> lock(outputMutex);
  printf("\n=== DESYNC DETECTED ===\n");
  printf("seed=%u frame=%d\n", info.seed, info.frame);
  printf("hashA=%08x hashB=%08x\n", info.hashA, info.hashB);
  printf("\n--- Component Hashes ---\n");
  printf("  %-12s A=%08x B=%08x %s\n", "rng", info.componentsA.rng, info.componentsB.rng,
         info.componentsA.rng != info.componentsB.rng ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "level", info.componentsA.level, info.componentsB.level,
         info.componentsA.level != info.componentsB.level ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "worm0", info.componentsA.worms[0], info.componentsB.worms[0],
         info.componentsA.worms[0] != info.componentsB.worms[0] ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "worm1", info.componentsA.worms[1], info.componentsB.worms[1],
         info.componentsA.worms[1] != info.componentsB.worms[1] ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "bobjects", info.componentsA.bobjects, info.componentsB.bobjects,
         info.componentsA.bobjects != info.componentsB.bobjects ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "bonuses", info.componentsA.bonuses, info.componentsB.bonuses,
         info.componentsA.bonuses != info.componentsB.bonuses ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "sobjects", info.componentsA.sobjects, info.componentsB.sobjects,
         info.componentsA.sobjects != info.componentsB.sobjects ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "nobjects", info.componentsA.nobjects, info.componentsB.nobjects,
         info.componentsA.nobjects != info.componentsB.nobjects ? "DIFFERS" : "ok");
  printf("  %-12s A=%08x B=%08x %s\n", "wobjects", info.componentsA.wobjects, info.componentsB.wobjects,
         info.componentsA.wobjects != info.componentsB.wobjects ? "DIFFERS" : "ok");

  printf("\n--- Settings ---\n");
  printf("  lives=%d blood=%d loadingTime=%d loadChange=%d gameMode=%u\n",
         settings.lives, settings.blood, settings.loadingTime, settings.loadChange, settings.gameMode);
  printf("  maxBonuses=%d regenerateLevel=%d shadow=%d\n",
         settings.maxBonuses, settings.regenerateLevel, settings.shadow);

  printf("\n--- Recent Inputs (last %zu frames) ---\n", info.recentInputs.size());
  printf("  %6s  %8s  %8s\n", "frame", "playerA", "playerB");
  for (auto& r : info.recentInputs) {
    printf("  %6u  %08b  %08b\n", r.frame, r.inputA, r.inputB);
  }
  printf("=== END DESYNC ===\n\n");
}

// Run one iteration of the fuzzer. Returns true if determinism held.
static bool runIteration(uint32_t seed, int maxFrames, int jitter, std::shared_ptr<Common> common) {
  FuzzerRng rng(seed);

  auto settings = std::make_shared<Settings>();
  randomizeSettings(rng, *settings);

  auto controllerA = std::make_unique<NetworkController>(common, settings, 0);
  auto controllerB = std::make_unique<NetworkController>(common, settings, 1);

  controllerA->setSkipWeaponSelection(true);
  controllerB->setSkipWeaponSelection(true);

  // Seed both games identically (different from fuzzer RNG)
  uint32_t gameSeed = rng.next();
  controllerA->game.rand.seed(gameSeed);
  controllerB->game.rand.seed(gameSeed);

  // Loopback wiring — packets go into pending queues
  std::queue<std::pair<uint32_t, uint8_t>> aToB, bToA;

  controllerA->setInputCallbacks(
      [&aToB](uint32_t frame, uint8_t input) { aToB.push({frame, input}); },
      [&bToA](uint32_t frame) -> int {
        if (!bToA.empty() && bToA.front().first == frame) {
          int val = bToA.front().second;
          bToA.pop();
          return val;
        }
        return -1;
      });

  controllerB->setInputCallbacks(
      [&bToA](uint32_t frame, uint8_t input) { bToA.push({frame, input}); },
      [&aToB](uint32_t frame) -> int {
        if (!aToB.empty() && aToB.front().first == frame) {
          int val = aToB.front().second;
          aToB.pop();
          return val;
        }
        return -1;
      });

  controllerA->focus();
  controllerB->focus();

  // Pre-inject initial inputs for input delay
  for (uint32_t i = 0; i < 3; ++i) {
    controllerA->injectRemoteInput(i, 0);
    controllerB->injectRemoteInput(i, 0);
  }

  // Jitter delay buffers: each entry is (deliverAtTick, frame, input)
  struct DelayedPacket {
    int deliverAtTick;
    uint32_t frame;
    uint8_t input;
  };
  std::vector<DelayedPacket> delayedToA, delayedToB;

  // Keep last N frames of inputs for diagnostics
  const int historySize = 10;
  std::vector<InputRecord> inputHistory;
  inputHistory.reserve(historySize);

  uint32_t lastFrameA = 0;

  // Each tick generates fresh random inputs. The NetworkController now only
  // captures input once per frame (on first entry), so changing inputs during
  // stalls won't cause desyncs.
  FuzzerRng inputRngA(rng.next());
  FuzzerRng inputRngB(rng.next());

  for (int tick = 0; tick < maxFrames; ++tick) {
    uint8_t inputA = randomInput(inputRngA);
    uint8_t inputB = randomInput(inputRngB);
    controllerA->setLocalControlState(inputA);
    controllerB->setLocalControlState(inputB);

    controllerA->process();
    controllerB->process();

    // Move packets from send queues into delay buffers
    while (!aToB.empty()) {
      auto [frame, input] = aToB.front();
      aToB.pop();
      int delay = jitter > 0 ? static_cast<int>(rng.range(jitter + 1)) : 0;
      delayedToB.push_back({tick + delay, frame, input});
    }
    while (!bToA.empty()) {
      auto [frame, input] = bToA.front();
      bToA.pop();
      int delay = jitter > 0 ? static_cast<int>(rng.range(jitter + 1)) : 0;
      delayedToA.push_back({tick + delay, frame, input});
    }

    // Deliver packets whose delay has expired
    for (auto it = delayedToB.begin(); it != delayedToB.end();) {
      if (it->deliverAtTick <= tick) {
        controllerB->injectRemoteInput(it->frame, it->input);
        it = delayedToB.erase(it);
      } else {
        ++it;
      }
    }
    for (auto it = delayedToA.begin(); it != delayedToA.end();) {
      if (it->deliverAtTick <= tick) {
        controllerA->injectRemoteInput(it->frame, it->input);
        it = delayedToA.erase(it);
      } else {
        ++it;
      }
    }

    // Record input history
    uint32_t frameA = controllerA->currentFrame();
    if (frameA != lastFrameA) {
      InputRecord rec{frameA, inputA, inputB};
      if (inputHistory.size() >= (size_t)historySize) {
        inputHistory.erase(inputHistory.begin());
      }
      inputHistory.push_back(rec);
      lastFrameA = frameA;
    }

    // Compare state when both advanced
    if (controllerA->currentFrame() == controllerB->currentFrame() &&
        controllerA->currentFrame() > 0) {
      uint32_t hashA = hashGameState(controllerA->game);
      uint32_t hashB = hashGameState(controllerB->game);

      if (hashA != hashB) {
        DesyncInfo info;
        info.seed = seed;
        info.frame = static_cast<int>(controllerA->currentFrame());
        info.hashA = hashA;
        info.hashB = hashB;
        info.componentsA = hashGameComponents(controllerA->game);
        info.componentsB = hashGameComponents(controllerB->game);
        info.recentInputs = inputHistory;
        printDesync(info, *settings);
        return false;
      }
    }
  }

  return true;
}

int main(int argc, char* argv[]) {
  FuzzerConfig cfg = parseArgs(argc, argv);

  // Generate base seed from time if not specified
  if (cfg.seed == 0) {
    cfg.seed = static_cast<uint32_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
  }

  printf("Desync Fuzzer: iterations=%d frames=%d jobs=%d jitter=%d baseSeed=%u\n",
         cfg.iterations, cfg.frames, cfg.jobs, cfg.jitter, cfg.seed);

  // Load game data once (read-only, shared across threads)
  precomputeTables();
  auto common = std::make_shared<Common>();
  FsNode tcRoot(FsNode("data") / "TC" / "openliero");
  common->load(std::move(tcRoot));

  std::atomic<int> passed{0};
  std::atomic<int> failed{0};
  std::atomic<int> nextIteration{0};
  std::mutex failedMutex;
  std::vector<uint32_t> failedSeeds;

  auto worker = [&]() {
    while (true) {
      int i = nextIteration.fetch_add(1);
      if (i >= cfg.iterations) break;

      uint32_t iterSeed = cfg.seed + static_cast<uint32_t>(i);
      bool ok = runIteration(iterSeed, cfg.frames, cfg.jitter, common);

      if (ok) {
        int p = passed.fetch_add(1) + 1;
        int total = p + failed.load();
        std::lock_guard<std::mutex> lock(outputMutex);
        printf("[%d/%d] seed=%u OK\n", total, cfg.iterations, iterSeed);
      } else {
        failed.fetch_add(1);
        std::lock_guard<std::mutex> lock(failedMutex);
        failedSeeds.push_back(iterSeed);
      }
    }
  };

  int numThreads = std::min(cfg.jobs, cfg.iterations);
  std::vector<std::thread> threads;
  threads.reserve(numThreads);

  auto startTime = std::chrono::steady_clock::now();

  for (int t = 0; t < numThreads; ++t) {
    threads.emplace_back(worker);
  }
  for (auto& t : threads) {
    t.join();
  }

  auto elapsed = std::chrono::steady_clock::now() - startTime;
  double seconds = std::chrono::duration<double>(elapsed).count();

  printf("\n=== Summary ===\n");
  printf("Passed: %d/%d\n", passed.load(), cfg.iterations);
  printf("Failed: %d/%d\n", failed.load(), cfg.iterations);
  printf("Time: %.1fs (%.1f iterations/s)\n", seconds, cfg.iterations / seconds);
  if (!failedSeeds.empty()) {
    printf("Failed seeds:");
    for (uint32_t s : failedSeeds) printf(" %u", s);
    printf("\n");
  }

  return failed.load() > 0 ? 1 : 0;
}
