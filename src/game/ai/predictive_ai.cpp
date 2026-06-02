// MSVC requires _USE_MATH_DEFINES before <cmath> to expose M_PI. The
// leading underscore is mandatory — it's the macro spelling MSVC
// looks for. NOLINT keeps clang-tidy's macro-naming rule from
// stripping the underscore.
// NOLINTNEXTLINE(readability-identifier-naming, bugprone-reserved-identifier)
#define _USE_MATH_DEFINES
#include "predictive_ai.hpp"
#include <cmath>

#include <cfloat>
#include <limits>
#include <sstream>
#include "../game.hpp"
#include "../gfx/blit.hpp"
#include "../gfx/renderer.hpp"
#include "../stats.hpp"

double TotalHealth(Worm* w) {
  if (w->lives == 0) return -10000.0;
  int h = std::max(w->health, 0);
  if (!w->visible) h = w->settings->health;
  return w->lives * (w->settings->health * 5.0 / 4.0) + h;
}

double TotalHealthNorm(Worm* w) { return TotalHealth(w) * 100.0 / w->settings->health; }

// 0..6*len(w->weapons)
double TotalAmmoWorth(Game& game, Worm* w) {
  double ammo_worth = 0;
  for (int i = 0; i < 5; ++i) {
    WormWeapon& weap = w->weapons[i];
    Weapon const& winfo = *weap.type;

    if (weap.loading_left > 0)
      ammo_worth += 3.0 - (weap.loading_left * 3.0) / winfo.loading_time;
    else
      ammo_worth += 3.0 + (weap.ammo * 3.0) / winfo.ammo;
  }

  return ammo_worth;
}

int ReadyWeapons(Game& game, Worm* w) {
  int count = 0;
  for (int i = 0; i < 5; ++i) {
    WormWeapon& weap = w->weapons[i];

    if (weap.loading_left == 0 && weap.delay_left < 70) ++count;
  }

  return count;
}

bool HasUsableWeapon(Game& game, Worm* w) {
  for (int i = 0; i < 5; ++i) {
    WormWeapon& weap = w->weapons[i];

    if (weap.loading_left <= 0) return true;
  }

  return false;
}

int WormDistance(Worm* from, Worm* to) {
  return VectorLength(Ftoi(to->pos.x) - Ftoi(from->pos.x), Ftoi(to->pos.y) - Ftoi(from->pos.y));
}

// langle in fixed point 0..128
double LangleToRadians(int langle) {
  double a = ((langle + Itof(32)) & (Itof(128) - 1)) * 0.04908738521234051935097880286374 / 65536.0;
  return a;
}

inline int NormalizedLangle(int langle) {
  if (langle < Itof(64)) langle = Itof(128) - langle;
  return langle;
}

inline double RadianDiff(double a, double b) {
  double aim_diff = b - a;
  while (aim_diff < -M_PI) aim_diff += 2 * M_PI;
  while (aim_diff > M_PI) aim_diff -= 2 * M_PI;
  return aim_diff;
}

double AimingDiff(Worm* from, Worm* to) {
  double xo = std::abs(to->pos.x - from->pos.x);
  double yo = to->pos.y - from->pos.y;

  double angle_to_target = xo != 0 || yo != 0 ? std::atan2(yo, xo) : 0;

  int aim = NormalizedLangle(from->aiming_angle);

  double current_aim = LangleToRadians(aim);

  double tolerance = M_PI / 8;
  double aim_diff = std::abs(RadianDiff(angle_to_target, current_aim));

  return std::max(aim_diff - tolerance, 0.0) / 6.0;
}

int Obstacles(Game& game, IVec2 from, IVec2 to) {
  typedef BasicVec<double, 2> dvec2;

  dvec2 org(from.x, from.y);
  dvec2 dir(to.x, to.y);
  dir -= org;

  double l = std::sqrt(dir.x * dir.x + dir.y * dir.y);
  dir /= l;

  int obst = 0;

  for (double d = 0; d < l; d += 2.0) {
    dvec2 p = org + dir * d;

    auto m = game.common->materials[game.level.CheckedPixelWrap((int)p.x, (int)p.y)];

    if (!m.Background()) {
      if (m.Dirt())
        obst += 2;
      else
        obst += 3;
    } else {
      obst += 0;
    }
  }

  return obst;
}

int Obstacles(Game& game, Worm* from, Worm* to) {
  double org_x = from->pos.x / 65536.0;
  double org_y = from->pos.y / 65536.0;
  double dir_x = (to->pos.x - from->pos.x) / 65536.0;
  double dir_y = (to->pos.y - from->pos.y) / 65536.0;

  double l = std::sqrt(dir_x * dir_x + dir_y * dir_y);
  dir_x /= l;
  dir_y /= l;

  int obst = 0;

  for (double d = 0; d < l; d += 2.0) {
    double px = org_x + dir_x * d;
    double py = org_y + dir_y * d;

    auto m = game.common->materials[game.level.CheckedPixelWrap((int)px, (int)py)];

    if (!m.Background()) {
      if (m.Dirt())
        obst += 2;
      else
        obst += 3;
    } else {
      obst += 0;
    }
  }

  return obst;
}

double AimingDiff(AiContext& context, Game& game, Worm* from, LevelCell* cell) {
  if (!cell || !cell->parent) return 1.0;

  auto org = context.dlevel.Coords(cell);
  auto orgl = context.dlevel.CoordsLevel(cell);

  double dirx = 0, diry = 0;
  auto* c = cell;

  // if (from->index == 0)
  if (true) {
    int count = 15;
    dirx = 1;
    while (c->parent && count-- > 0) {
      c = (LevelCell*)c->parent;
      auto path = context.dlevel.CoordsLevel(c);

      if ((path.x != orgl.x && path.y != orgl.y) && Obstacles(game, orgl, path) > 4) break;

      dirx = std::abs(path.x - orgl.x);
      diry = path.y - orgl.y;
    }
  } else {
    int count = 10;

    while (count-- > 0) {
      c = (LevelCell*)c->parent;
      if (!c) break;

      auto path = context.dlevel.Coords(c);

      int xdiff = std::abs(path.x - org.x);
      int ydiff = path.y - org.y;

      double len = std::sqrt(xdiff * xdiff + ydiff * ydiff);
      dirx += xdiff / len;
      diry += ydiff / len;
    }
  }

  int aim = NormalizedLangle(from->aiming_angle);
  double current_aim = LangleToRadians(aim);

  double angle_to_target = std::atan2(diry, dirx);

  double tolerance = M_PI / 8;
  double aim_diff = std::abs(RadianDiff(angle_to_target, current_aim));

  return std::max(aim_diff - tolerance, 0.0) / 6.0;
}

inline int WeaponChangeOffset(int wanted_weapon, int current_weapon) {
  int offset = wanted_weapon - current_weapon;
  offset = ((offset + 2 + 5) % 5) - 2;
  return offset;
}

enum MutationType { kMtIdentity, kMtRange, kMtOptimize };

struct MutationStrategy {
  MutationStrategy(MutationType type, uint32_t start = 0, uint32_t stop = 0)
      : type(type), start(start), stop(stop) {}

  static MutationStrategy Identity() { return MutationStrategy(kMtIdentity); }

  static MutationStrategy Optimize() { return MutationStrategy(kMtOptimize); }

  MutationType type;
  uint32_t start;
  uint32_t stop;
};

InputState Generate(FollowAI& ai, Rand& rand, InputContext& prev) {
  return ai.model.Random(prev, rand);
}

double Sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

// psigmoid(0) = 0
// psigmoid(1) ~= 0.5
// psigmoid(inf) = 1
double Psigmoid(double x) { return (Sigmoid(x) - 0.5) * 2.0; }

LevelCell* AiContext::PathFind(int x, int y) {
  auto* cell = dlevel.CellFromPx(x, y);
  bool path = dlevel.Run([=] { return cell->state == PathNode::kClosed; }, LevelCellSucc());
  return path ? cell : 0;
}

double EvaluateState(FollowAI& ai, Worm* me, Game& game, InputContext& context, Worm* target,
                     Game& org_game, std::size_t index) {
  double score = 0;

  Weights weights = ai.weights;

  int posx = Ftoi(me->pos.x), posy = Ftoi(me->pos.y);
  auto* worm_cell = ai.PathFind(posx, posy);
  Worm* me_org = org_game.WormByIdx(me->index);

  double len = 200.0;
  if (worm_cell) {
    double optimal_dist = 10.0;

    if (ReadyWeapons(org_game, org_game.WormByIdx(me->index)) <= 1) {
      optimal_dist = 50.0;
    }

    double d = std::max(std::abs(worm_cell->g / 256.0 - optimal_dist) - 10.0, 0.0) *
               weights.distance_weight;
    len *= Psigmoid(d / 100.0);
  } else {
    len += WormDistance(me, target) / 10.0;
  }

  if (me_org->steerable_count == 1) {
    auto* missile_cell = ai.dlevel.CellFromPx(me->steerable_sum_x, me->steerable_sum_y);
    bool missile_path =
        ai.dlevel.Run([=] { return missile_cell->state == PathNode::kClosed; }, LevelCellSucc());

    if (missile_path && worm_cell && missile_cell->g < worm_cell->g) {
      double closer = (double)worm_cell->g - missile_cell->g;
      score += Psigmoid((closer / (double)worm_cell->g)) * 100.0 * weights.missile_weight;
    }
  }

  double me_health = TotalHealthNorm(me);
  double target_health = TotalHealthNorm(target);

  score += me_health * weights.health_weight * weights.defense_weight;
  score -= target_health * weights.health_weight;

  if (game.settings->game_mode == Settings::kGmHoldazone &&
      game.holdazone.holder_idx == me->index) {
    double aim_diff = AimingDiff(me, target);
    score -= aim_diff * 2.0 * weights.aim_weight;
  } else {
    double aim_diff = AimingDiff(ai, game, me, worm_cell);
    score -= aim_diff * 2.0 * weights.aim_weight;
  }

  {
    double me_ammo_worth = TotalAmmoWorth(game, me);

    score += me_ammo_worth * 2.0 * weights.ammo_weight;
  }

  if (game.settings->game_mode == Settings::kGmHoldazone) {
    double scale = 1.0 / 4.0;
    if (game.holdazone.holder_idx != me->index) scale = 1.0;

    score -= len * scale;
  } else if (game.settings->game_mode == Settings::kGmGameOfTag) {
    if (game.last_killed_idx >= 0 && game.last_killed_idx != me->index)
      score += len;
    else
      score -= len;
  } else {
    score -= len;
  }

  if (!me->visible && !me->ready) {
  } else if (me->visible) {
  }

  if (game.settings->game_mode == Settings::kGmHoldazone) {
    if (game.holdazone.holder_idx == me->index) score += 50.0;
    if (game.holdazone.contender_idx == me->index)
      score += game.holdazone.contender_frames * 30.0 / 70.0;

    if (game.holdazone.holder_idx == target->index) score -= 50.0;
    if (game.holdazone.contender_idx == target->index)
      score -= game.holdazone.contender_frames * 30.0 / 70.0;
  }

  return score;
}

double EvaluateResult::WeightedScore() const {
  double r = 0.0;

  for (std::size_t i = 1; i < score_over_time.size(); ++i) {
    double weight = double(score_over_time.size() - i) / score_over_time.size();

    double diff = score_over_time[i] * weight;
    r += diff;
  }

  return r + future_score;
}

void SimpleAI::Process(Game& game, Worm& worm) {
  Worm* target = game.WormByIdx(worm.index ^ 1);

  auto cs = worm.control_states;

  if (!worm.visible) {
    cs.Set(Worm::kFire, true);
  } else {
    int aim = NormalizedLangle(worm.aiming_angle);
    double current_aim = LangleToRadians(aim);

    double dirx = std::abs(target->pos.x - worm.pos.x);
    double diry = target->pos.y - worm.pos.y;
    double angle_to_target = std::atan2(diry, dirx);

    double tolerance = 2 * M_PI / 32.0;

    double aim_diff = RadianDiff(angle_to_target, current_aim);

    bool fire =
        aim_diff >= -tolerance && aim_diff <= tolerance && Obstacles(game, &worm, target) < 4;
    {
      cs = initial;
      cs.Set(Worm::kDown, aim_diff < -tolerance);
      cs.Set(Worm::kUp, aim_diff > tolerance);
      cs.Set(Worm::kFire, fire || initial[Worm::kFire]);
      cs.Set(Worm::kChange, false);

      if (cs[Worm::kFire] && target->pos.x < worm.pos.x && worm.direction != 0) {
        cs.Set(Worm::kLeft, true);
        cs.Set(Worm::kRight, false);
      } else if (cs[Worm::kFire] && target->pos.x > worm.pos.x && worm.direction != 1) {
        cs.Set(Worm::kLeft, false);
        cs.Set(Worm::kRight, true);
      }
    }
  }

  worm.control_states = cs;
}

void Evaluate(EvaluateResult& result, FollowAI& ai, Worm* me, Game& game, Worm* target, Plan& plan,
              std::size_t plan_size, MutationStrategy const& ms) {
  Game copy(game);
  copy.PostClone(game);
  copy.quick_sim = true;

  Worm* me_copy = copy.WormByIdx(me->index);
  Worm* target_copy = copy.WormByIdx(target->index);

  FollowAI* target_ai = 0;

  auto context = ai.current_context;
  InputContext target_context;
  if (target_ai) target_context = target_ai->current_context;

  SimpleAI simple_ai;

  simple_ai.initial = target_copy->control_states;

  double prev_s = EvaluateState(ai, me_copy, copy, context, target_copy, game, 0);

  if (result.score_over_time.size() < plan_size + 1) result.score_over_time.resize(plan_size + 1);
  result.score_over_time[0] = 0.0;

  std::vector<int> weapon_changes_left;
  if (ms.type == kMtOptimize) {
    weapon_changes_left.resize(plan_size);
    int changes_left = 0;
    for (std::size_t j = std::min(plan_size, plan.size()); j-- > 0;) {
      if (plan[j].IsFiring()) {
        changes_left = 0;
      } else if (plan[j].IsNeutral()) {
        ++changes_left;
      }
      weapon_changes_left[j] = changes_left;
    }
  }

  for (std::size_t i = 0; i < plan_size; ++i) {
    if (plan.size() <= i) {
      plan.push_back(Generate(ai, ai.rand, context));
    }

    if (ms.type == kMtIdentity) {
      // Do nothing
    } else if (ms.type == kMtRange) {
      if (i >= ms.start && i < ms.stop) {
        plan[i] = Generate(ai, ai.rand, context);
      }
    } else if (ms.type == kMtOptimize) {
      // If current InputState is move/jump/fire neutral, make it change weapon to a loading weapon
      // as long as there's time to change back

      if (plan[i].IsNeutral() && me_copy->weapons[me_copy->current_weapon].loading_left == 0) {
        // Out of all loading weapons that are at most weaponChangesLeft[i] - 1 away from
        // a loaded weapon, pick the one that is closest to loaded.
        for (int w = 0; w < 5; ++w) {
          if (me_copy->weapons[w].loading_left > 0) {
            int distance_from_current = std::abs(WeaponChangeOffset(w, me_copy->current_weapon));
            int loaded_distance = 5;
            for (int lw = 0; lw < 5; ++lw) {
              if (me_copy->weapons[lw].loading_left == 0) {
                loaded_distance = std::min(loaded_distance, std::abs(WeaponChangeOffset(lw, w)));
              }
            }

            int total_distance = distance_from_current + loaded_distance;
            if (total_distance <= weapon_changes_left[i]) {
              plan[i] = InputState::Compose(InputState::kChangeWeapon, w, 0, 0);
              break;
            }
          }
        }
      }
    }

    me_copy->control_states = context.Update(plan[i], copy, me_copy, ai);

    if (target_ai && target_ai->best && i < target_ai->best->plan.size())
      target_copy->control_states =
          target_context.Update(target_ai->best->plan[i], copy, target_copy, *target_ai);
    else
      simple_ai.Process(copy, *target_copy);

    copy.ProcessFrame();

    double s = EvaluateState(ai, me_copy, copy, context, target_copy, game, i + 1);

    if (game.settings->ai_traces) {
      int t = 119 - (int)(i * (119 - 104 + 1) / plan_size);

      ai.evaluate_positions.push_back(std::make_tuple(IVec2(me_copy->pos.x, me_copy->pos.y), t));
    }

    result.score_over_time[i + 1] = s - prev_s;

    prev_s = s;
  }
}

void Mutate(EvaluateResult& result, FollowAI& ai, Game& game, Worm& worm, Worm* target,
            Plan& candidate, EvaluateResult const& prev_result) {
  MutationStrategy ms(kMtRange, 0, (uint32_t)candidate.size());

  {
    // Find the minimum suffix sum
    uint32_t j = uint32_t(prev_result.score_over_time.size() - 1);

    double sum = prev_result.score_over_time[j];
    double min = sum;
    auto minj = j;

    while (j-- > 0) {
      sum += prev_result.score_over_time[j];
      if (sum < min) {
        minj = j;
        min = sum;
      }
    }

    if (ai.rand(8) < 7) {
      ms.stop = ai.rand(minj);
      ms.start = ai.rand(std::max(ms.stop, uint32_t(10)) - 10, ms.stop);
    } else {
      ms.start = ai.rand(0, (uint32_t)candidate.size());
      ms.stop = ai.rand(ms.start, std::min(ms.start + 10, (uint32_t)candidate.size()));
    }
  }

  Evaluate(result, ai, &worm, game, target, candidate, game.settings->ai_frames, ms);
}

void FollowAI::DrawDebug(Game& game, Worm const& worm, Renderer& renderer, int offs_x, int offs_y) {
  for (auto& p : evaluate_positions) {
    IVec2 v;
    PalIdx t;
    std::tie(v, t) = p;
    renderer.bmp.SetPixel(Ftoi(v.x) + offs_x, Ftoi(v.y) + offs_y, t);
  }
}

#if AI_THREADS

struct MutateWork : Work {
  int evaluations;
  int maxEvaluations;
  Plan best;
  Worm* worm;
  Game game;

  virtual void run() {
    while (evaluations < maxEvaluations)  // game.settings->aiMutations + 1
    {
      auto candidate = best;
      EvaluateResult result(mutate(*this, game, worm, target, candidate, 1, prevResult));
      ++evaluations;

      double weightedScore = result.weightedScore();
      if (weightedScore >= bestScore) {
        best = candidate;
        bestScore = weightedScore;
        prevResult = std::move(result);
        prevResultAge = 0;
      }
    }
  }
};

#endif

void FollowAI::Process(Game& game, Worm& worm) {
  Common& common = *game.common;

  Worm* target = game.worms[worm.index ^ 1].get();

  {
    int targetx = Ftoi(target->pos.x), targety = Ftoi(target->pos.y);

    if (game.settings->game_mode == Settings::kGmHoldazone) {
      targetx = game.holdazone.rect.CenterX();
      targety = game.holdazone.rect.CenterY();
    }

    dlevel.Build(game.level, common);
    auto* target_cell = dlevel.CellFromPx(targetx, targety);
    target_cell->cost = 1;
    dlevel.SetOrigin(target_cell);
  }

  Update(*this, worm);

  double best_score = -std::numeric_limits<double>::infinity();
  evaluate_positions.clear();

  {
    unsigned int cand_idx;

    evaluation_budget += (game.settings->ai_mutations + 1) * game.settings->ai_frames;

    std::vector<std::pair<double, int>> prio;

    for (cand_idx = 0; cand_idx < cand_plan.size(); ++cand_idx) {
      auto& cand = cand_plan[cand_idx];

      if (cand.prev_result_age < 2 && !cand.prev_result.score_over_time.empty()) {
      } else if (cand.prev_result.score_over_time.empty()) {
        Evaluate(cand.prev_result, *this, &worm, game, target, cand.plan, game.settings->ai_frames,
                 testing ? MutationStrategy::Optimize() : MutationStrategy::Identity());
        evaluation_budget -= game.settings->ai_frames;
        cand.prev_result_age = 0;
      } else {
        Evaluate(cand.prev_result, *this, &worm, game, target, cand.plan,
                 game.settings->ai_frames / 2,
                 testing ? MutationStrategy::Optimize() : MutationStrategy::Identity());
        evaluation_budget -= game.settings->ai_frames / 2;
        cand.prev_result_age = 0;
      }

      double weighted_score = cand.prev_result.WeightedScore();
      if (weighted_score >= best_score) {
        best_score = weighted_score;
        best = &cand;
      }

      prio.push_back(std::make_pair(weighted_score, cand_idx));
    }

    std::sort(prio.begin(), prio.end(),
              [](std::pair<double, int> const& a, std::pair<double, int> const& b) {
                return a.first > b.first;
              });

    for (cand_idx = 0; evaluation_budget > 0;) {
      auto& cand = cand_plan[prio[cand_idx].second];
      auto candidate = cand.plan;
      EvaluateResult result;
      Mutate(result, *this, game, worm, target, candidate, cand.prev_result);
      evaluation_budget -= game.settings->ai_frames;

      double weighted_score = result.WeightedScore();
      if (weighted_score > best_score) {
        cand.plan = std::move(candidate);
        best = &cand;
        best_score = weighted_score;
        cand.prev_result = std::move(result);
        cand.prev_result_age = 0;
      } else {
        cand_idx = (cand_idx + 1) % cand_plan.size();
      }
    }
  }

  worm.control_states = current_context.Update(best->plan[0], game, &worm, *this);

  if (frame < 70 * 2 * 60) {
    model.Update(current_context, best->plan[0]);
  }

  for (auto& p : cand_plan) {
    p.plan.erase(p.plan.begin());

    // Shift result
    p.prev_result.score_over_time.erase(p.prev_result.score_over_time.begin());
    p.prev_result.score_over_time.push_back(0.0);
    ++p.prev_result_age;
  }

  ++frame;
}

Worm::ControlState InputContext::Update(InputState new_state, Game& game, Worm* worm,
                                        AiContext& ai_context) {
  Worm::ControlState cs;

  if (worm->visible && wanted_weapon != worm->current_weapon) {
    int offset = WeaponChangeOffset(wanted_weapon, worm->current_weapon);
    cs.Set(Worm::kLeft, offset < 0);
    cs.Set(Worm::kRight, offset > 0);
    cs.Set(Worm::kChange, true);
  } else {
    int pa, pb, pc;
    switch (new_state.Decompose(pa, pb, pc)) {
      case InputState::kMoveJumpFire: {
        cs.Set(Worm::kUp, (pa >> 1) & 1);
        cs.Set(Worm::kDown, pa & 1);
        cs.Set(Worm::kLeft, (pb >> 1) & 1);
        cs.Set(Worm::kRight, pb & 1);
        cs.Set(Worm::kFire, (pc >> 1) & 1);
        cs.Set(Worm::kJump, pc & 1);
        break;
      }

      case InputState::kChangeWeapon: {
        if (wanted_weapon != pa) {
          wanted_weapon = pa;
          cs.Set(Worm::kChange, true);
        }
        break;
      }

      case InputState::kRopeUpDown: {
        cs.Set(Worm::kUp, (pa >> 1) & 1);
        cs.Set(Worm::kDown, pa & 1);
        cs.Set(Worm::kChange, 1);
        if (pa == 0) cs.Set(Worm::kJump, 1);
        break;
      }
    }
  }

  current_state = new_state;

  if (!worm->visible && !worm->ready)
    ++hidden_frames;
  else
    hidden_frames = 0;

  facing_enemy = (game.worms[worm->index ^ 1]->pos.x > worm->pos.x) == worm->direction;
  ninjarope_out = worm->ninjarope.out;

  return cs;
}

void TransToM(Weights& weights, double& p, int pa, int pb, int pc, int facing_enemy,
              int ninjarope_out, int pa2, int pb2, int pc2) {
  assert(pa < 3 && pb < 4 && pc < 4);
  assert(pa2 < 3 && pb2 < 4 && pc2 < 4);
  assert(facing_enemy < 2 && ninjarope_out < 2);

  if (pa == 0) p *= Select(pa2, 0.1, 0.45, 0.45);    // Start aiming
  if (pa == 1) p *= Select(pa2, 0.025, 0.9, 0.075);  // Aiming down
  if (pa == 2) p *= Select(pa2, 0.025, 0.075, 0.9);  // Aiming up

  if (pb == 0) p *= Select(pb2, 0.05, 0.4, 0.4, 0.15);  // Not moving

  if (pb == 1) p *= Select(pb2, 0.005, 0.965, 0.005, 0.025);  // Moving right
  if (pb == 2) p *= Select(pb2, 0.005, 0.005, 0.965, 0.025);  // Moving left
  if (pb == 3) p *= Select(pb2, 0.1, 0.35, 0.35, 0.2);        // Digging

  // Fire
  double start_shoot_facing_p = 0.15 * weights.firing_weight;
  double start_shoot_unfacing_p = 0.03 * weights.firing_weight;
  double start_shoot_p = facing_enemy ? start_shoot_facing_p : start_shoot_unfacing_p;
  p *= Select((pc & 2) | ((pc2 & 2) >> 1), 1.0 - start_shoot_p, start_shoot_p, 0.4, 0.6);

  // Jump
  double start_jump = ninjarope_out ? 0.015 : 0.1;
  p *= Select(((pc & 1) << 1) | (pc2 & 1), 1.0 - start_jump, start_jump,  // 0 -> 0, 0 -> 1
              0.999, 0.001);                                              // 1 -> 0, 1 -> 1
}

TransModel::TransModel(Weights& weights, bool testing) {
  for (int i = 0; i < this->kStates; ++i) {
    int facing_enemy, ninjarope_out;
    auto prev = InputContext::Unpack(i, facing_enemy, ninjarope_out);
    int pa, pb, pc;
    auto type = prev.Decompose(pa, pb, pc);

    double sum2 = 0.0;

    for (int j = 0; j < this->kFreeStates; ++j) {
      int pa2, pb2, pc2;
      auto type2 = InputState(j).Decompose(pa2, pb2, pc2);

      double p = 1;

      if (type == InputState::kMoveJumpFire) {
        double m = 0.95, c = 0.03, r = 0.02;

        c += 0.03;
        m -= 0.03;

        if (type2 == InputState::kMoveJumpFire) {
          p *= m;
          TransToM(weights, p, pa, pb, pc, facing_enemy, ninjarope_out, pa2, pb2, pc2);
        } else if (type2 == InputState::kChangeWeapon) {
          p *= c;
          p *= 1.0 / 5.0;  // Each weapon has equal probability
        } else if (type2 == InputState::kRopeUpDown) {
          p *= r;
          p *= Select(pa2, 0.98, 0.01, 0.01);  // TODO: Rope out
        }
      } else if (type == InputState::kChangeWeapon) {
        if (type2 == InputState::kMoveJumpFire) {
          // Same as from the idle m-state
          p *= 0.97;
          TransToM(weights, p, 0, 0, 0, facing_enemy, ninjarope_out, pa2, pb2, pc2);
        } else if (type2 == InputState::kChangeWeapon) {
          p *= 0.001;      // Very little chance
          p *= 1.0 / 5.0;  // Each weapon has equal probability
        } else if (type2 == InputState::kRopeUpDown) {
          p *= 0.029;
          p *= Select(pa2, 0.98, 0.01, 0.01);  // TODO: Rope out
        }
      } else if (type == InputState::kRopeUpDown) {
        if (type2 == InputState::kMoveJumpFire) {
          // Same as from the idle state
          p *= 0.95;
          TransToM(weights, p, 0, 0, 0, facing_enemy, ninjarope_out, pa2, pb2, pc2);
        } else if (type2 == InputState::kChangeWeapon) {
          p *= 0.049;
          p *= 1.0 / 5.0;  // Each weapon has equal probability
        } else if (type2 == InputState::kRopeUpDown) {
          p *= 0.001;                          // Very little chance
          p *= Select(pa2, 0.98, 0.01, 0.01);  // TODO: Rope out
        }
      }

      sum2 += p;

      trans[i][j] = p;
    }

    auto& v = trans[i];
    double sum = std::accumulate(v, v + this->kFreeStates, 0.0);

    if (sum < 0.999 || sum > 1.0001) printf("%d: %f\n", i, sum);
  }
}

void AiContext::Update(FollowAI& ai, Worm& worm) {
  double presence = 0, damage = 0;

  if (worm.visible) {
    presence = 4.0 / (70.0 * 5.0);
  }

  double hp = TotalHealthNorm(&worm);

  if (hp < prev_hp) {
    damage = (prev_hp - hp);
  }

  IncArea(worm.pos.x, worm.pos.y, presence, damage);

  for (int y = 0; y < kHeight; ++y)
    for (int x = 0; x < kWidth; ++x) {
      state[x][y].presence = std::max(state[x][y].presence - 0.5 / (70.0 * 5.0), 0.0);
    }
}
