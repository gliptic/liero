#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <functional>
#include <numeric>
#include "../math.hpp"
#include "../rand.hpp"
#include "../worm.hpp"
#include "dijkstra.hpp"
#include "math/rect.hpp"
#include "work_queue.hpp"

struct InputState {
  enum Type {
    kMoveJumpFire = 0,  // 48, aabb0cc, a != 3
    kChangeWeapon = 1,  // 5,  aaa, 0..4, translated to 0010100 or 0001100
    kRopeUpDown = 2,    // 3,  aa00110, a != 3
  };

  InputState(Worm* w) {
    auto cs = w->control_states;
    auto v = cs.Pack();

    if (!cs[Worm::kChange]) {
      // MoveJumpFire
      idx = (v >> 2) << 1;
      if (idx > 48) idx -= (1 << 5);
      idx = 2;
      idx |= (v & 1);
    } else {
      if (!cs[Worm::kJump]) {
        // ChangeWeapon
        idx = 48 + w->current_weapon;
      } else {
        // RopeUpDown
        idx = 48 + 5 + (v >> 5);
      }
    }
  }

  InputState(int idx = 0) : idx(idx) {}

  int idx;  // 0..56

  bool IsNeutral() const {
    int pa, pb, pc;
    auto type = Decompose(pa, pb, pc);
    return type == kChangeWeapon;
  }

  bool IsFiring() const {
    int dummy, pc;
    return Decompose(dummy, dummy, pc) == kMoveJumpFire && ((pc >> 1) & 1);
  }

  Type Decompose(int& pa, int& pb, int& pc) const {
    int i = idx;
    if (i < 48) {
      pa = i >> 4;
      pb = (i >> 2) & 3;
      pc = i & 3;
      return kMoveJumpFire;
    }
    i -= 48;

    if (i < 5) {
      pa = i;
      return kChangeWeapon;
    }
    i -= 5;

    if (i < 3) {
      pa = i;
      return kRopeUpDown;
    }

    assert(false);
    return kMoveJumpFire;
  }

  static InputState Compose(Type type, int pa, int pb, int pc) {
    int idx = 0;
    switch (type) {
      case kMoveJumpFire: {
        idx = (pa << 4) | (pb << 2) | pc;
        break;
      }

      case kChangeWeapon: {
        idx = 48 + pa;
        break;
      }

      case kRopeUpDown: {
        idx = 48 + 5 + pa;
        break;
      }
    }

    return InputState(idx);
  }
};

typedef std::vector<InputState> Plan;

template <typename T>
inline T Select(int n, T first) {
  assert(n == 0);
  return first;
}

template <typename T>
inline T Select(int n, T first, T a) {
  if (n == 0) return first;
  return Select(n - 1, a);
}

template <typename T>
inline T Select(int n, T first, T a, T b) {
  if (n == 0) return first;
  return Select(n - 1, a, b);
}

template <typename T>
inline T Select(int n, T first, T a, T b, T c) {
  if (n == 0) return first;
  return Select(n - 1, a, b, c);
}

struct AiContext;

struct InputContext {
  InputContext() : wanted_weapon(0), hidden_frames(0), facing_enemy(0), ninjarope_out(0) {}

  Worm::ControlState Update(InputState new_state, Game& game, Worm* worm, AiContext& ai_context);

  int Pack() {
    int i = ninjarope_out;
    i = i * 2 + facing_enemy;
    i = i * 56 + current_state.idx;
    return i;
  }

  static InputState Unpack(int idx, int& facing_enemy, int& ninjarope_out) {
    int s = idx % 56;
    idx /= 56;
    facing_enemy = idx % 2;
    idx /= 2;
    ninjarope_out = idx;
    return InputState(s);
  }

  static int const kSize = 56 * 2 * 2;

  // Free part
  InputState current_state;

  // Dependent part
  int wanted_weapon;
  int hidden_frames;
  int facing_enemy;
  int ninjarope_out;
};

template <int States, int FreeStates>
struct Model {
  static int const kStates = States;
  static int const kFreeStates = FreeStates;
  double trans[States][FreeStates];

  int Random(int context, Rand& rand) {
    assert(context < States);
    auto& v = trans[context];

    double max = std::accumulate(v, v + FreeStates, 0.0);
    double el = rand.GetDouble(max);

    for (int i = 0; i < FreeStates; ++i) {
      el -= v[i];
      if (el < 0.0) return i;
    }

    return FreeStates - 1;
  }
};

struct Weights {
  Weights()
      : health_weight(1.0),
        aim_weight(1.0),
        distance_weight(1.0),
        ammo_weight(1.0),
        missile_weight(1.0),
        defense_weight(1.3),
        firing_weight(1.0) {}

  double health_weight, aim_weight, distance_weight, ammo_weight, missile_weight;
  double defense_weight, firing_weight;
};

struct TransModel : Model<InputContext::kSize, 56> {
  TransModel(Weights& weights, bool testing);

  void Update(InputContext context, InputState v) { trans[context.Pack()][v.idx] += 0.005; }

  InputState Random(InputContext context, Rand& rand) {
    return InputState(Model<InputContext::kSize, 56>::Random(context.Pack(), rand));
  }
};

struct CellState {
  double presence;
  double damage;  // Health decrease in this cell
};

struct FollowAI;

struct AiContext {
  static int const kWidth = (504 + 31) >> 5;
  static int const kHeight = (350 + 31) >> 5;

  AiContext() : prev_hp(0), max_damage(0), max_presence(0) {}

  DijkstraLevel dlevel;

  CellState state[kWidth][kHeight];

  int prev_hp;
  double max_damage, max_presence;

  void IncArea(int fx, int fy, double presence, double damage) {
    int wx = Ftoi(fx) >> 5;
    int wy = Ftoi(fy) >> 5;

    for (int y = wy - 1; y <= wy + 1; ++y)
      for (int x = wx - 1; x <= wx + 1; ++x) {
        if (y >= 0 && y < kHeight && x >= 0 && x < kWidth) {
          double d = 1.0;
          if (x != wx) d *= 0.5;
          if (y != wy) d *= 0.5;
          auto& c = state[x][y];
          c.presence = d * presence;
          c.damage += d * damage;
          max_damage = std::max(max_damage, c.damage);
          max_presence = std::max(max_presence, c.presence);
        }
      }
  }

  CellState& Cell(int fx, int fy) {
    int wx = Ftoi(fx) >> 5;
    int wy = Ftoi(fy) >> 5;

    wx = std::max(std::min(wx, kWidth), 0);
    wy = std::max(std::min(wy, kHeight), 0);

    return state[wx][wy];
  }

  void Update(FollowAI& ai, Worm& worm);
  LevelCell* PathFind(int x, int y);
};

struct EvaluateResult {
  EvaluateResult() : future_score(0.0) {}

  double WeightedScore() const;

  std::vector<double> score_over_time;
  double future_score;
};

struct SimpleAI : WormAI {
  void Process(Game& game, Worm& worm);

  Worm::ControlState initial;
};

struct AIThread {
  AIThread() : th(0) {}

  SDL_Thread* th;
};

struct CandPlan {
  CandPlan() : prev_result_age(0) {}

  Plan plan;
  EvaluateResult prev_result;
  int prev_result_age;
};

struct FollowAI : WormAI, AiContext {
  FollowAI(Weights weights, int cand_pop_size, bool testing, FollowAI* target_ai_init = 0)
      : frame(0),
        model(weights, testing),
        evaluation_budget(0),
        effect_scaler(0),
        target_ai(target_ai_init),
        cand_plan(cand_pop_size),
        best(0),
        testing(testing),
        weights(weights)
#if AI_THREADS
        ,
        workQueue(2)
#endif
  {
  }

  ~FollowAI() {}

  void Process(Game& game, Worm& worm);

  void DrawDebug(Game& game, Worm const& worm, Renderer& renderer, int offs_x, int offs_y);

  Rand rand;
  int frame;
  InputContext current_context;
  TransModel model;
  int evaluation_budget;

  std::vector<std::tuple<IVec2, PalIdx>> evaluate_positions;

  std::vector<double> neg_effect, pos_effect;
  int effect_scaler;

  FollowAI* target_ai;

  std::vector<CandPlan> cand_plan;
  CandPlan* best;

  bool testing;

#if AI_THREADS
  WorkQueue workQueue;
#endif

  Weights weights;
};
