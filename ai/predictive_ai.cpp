#include "predictive_ai.hpp"

#include "game.hpp"
#include "gfx/blit.hpp"
#include "gfx/renderer.hpp"
#include "stats.hpp"
#include <sstream>

double totalHealth(Worm* w)
{
	if (w->lives == 0)
		return -10000.0;
	int h = std::max(w->health, 0);
	if (!w->visible)
		h = w->settings->health;
	return w->lives * (w->settings->health * 5.0 / 4.0) + h;
}

double totalHealthNorm(Worm* w)
{
	return totalHealth(w) * 100.0 / w->settings->health;
}

// 0..6*len(w->weapons)
double totalAmmoWorth(Game& game, Worm* w)
{
	double ammoWorth = 0;
	for (int i = 0; i < 5; ++i)
	{
		WormWeapon& weap = w->weapons[i];
		Weapon& winfo = game.common->weapons[weap.id];

		if (weap.loadingLeft > 0)
			ammoWorth += 3.0 - (weap.loadingLeft * 3.0) / winfo.loadingTime;
		else
			ammoWorth += 3.0 + (weap.ammo * 3.0) / winfo.ammo;
	}

	return ammoWorth;
}

bool hasUsableWeapon(Game& game, Worm* w)
{
	for (int i = 0; i < 5; ++i)
	{
		WormWeapon& weap = w->weapons[i];
		Weapon& winfo = game.common->weapons[weap.id];

		if (weap.loadingLeft <= 0)
			return true;
	}

	return false;
}

int wormDistance(Worm* from, Worm* to)
{
	return vectorLength(
		ftoi(to->x) - ftoi(from->x),
		ftoi(to->y) - ftoi(from->y));
}

// langle in fixed point 0..128
double langleToRadians(int langle)
{
	double a = ((langle + itof(32)) & (itof(128)-1)) * 0.04908738521234051935097880286374 / 65536.0;
	return a;
}

inline int normalizedLangle(int langle)
{
	if (langle < itof(64)) langle = itof(128) - langle;
	return langle;
}

double const pi = 3.141592653589793;

inline double radianDiff(double a, double b)
{
	double aimDiff = b - a;
	while (aimDiff < -pi) aimDiff += 2 * pi;
	while (aimDiff > pi) aimDiff -= 2 * pi;
	return aimDiff;
}

double aimingDiff(Worm* from, Worm* to)
{
	double xo = std::abs(to->x - from->x);
	double yo = to->y - from->y;

	double angleToTarget = xo != 0 || yo != 0 ? std::atan2(yo, xo) : 0;

	int aim = normalizedLangle(from->aimingAngle);

	double currentAim = langleToRadians(aim);

	double tolerance = pi / 8;
	double aimDiff = std::abs(radianDiff(angleToTarget, currentAim));

	return std::max(aimDiff - tolerance, 0.0) / 6.0;
}

double aimingDiff(AiContext& context, Worm* from, level_cell* cell)
{
	if (!cell || !cell->parent)
		return 1.0;

	auto org = context.dlevel.coords(cell);

	double dirx = 0, diry = 0;
	auto* c = cell;
	int count = 10;
	while (count-- > 0)
	{
		c = (level_cell*)c->parent;
		if (!c)
			break;

		auto path = context.dlevel.coords(c);

		int xdiff = std::abs(path.first - org.first);
		int ydiff = path.second - org.second;

		double len = std::sqrt(xdiff*xdiff + ydiff*ydiff);
		dirx += xdiff / len;
		diry += ydiff / len;
	}

	int aim = normalizedLangle(from->aimingAngle);
	double currentAim = langleToRadians(aim);

	double angleToTarget = std::atan2(diry, dirx);

	double tolerance = pi / 8;
	double aimDiff = std::abs(radianDiff(angleToTarget, currentAim));

	return std::max(aimDiff - tolerance, 0.0) / 6.0;
}

int obstacles(Game& game, Worm* from, Worm* to)
{
	double orgX = from->x / 65536.0;
	double orgY = from->y / 65536.0;
	double dirX = (to->x - from->x) / 65536.0;
	double dirY = (to->y - from->y) / 65536.0;

	double l = std::sqrt(dirX*dirX + dirY*dirY);
	dirX /= l;
	dirY /= l;

	int obst = 0;

	for (double d = 0; d < l; d += 2.0)
	{
		double px = orgX + dirX * d;
		double py = orgY + dirY * d;

		auto m = game.common->materials[
			game.level.checkedPixelWrap((int)px, (int)py)];

		if (!m.background())
		{
			if (m.dirt())
				obst += 2;
			else
				obst += 3;
		}
		else
		{
			obst += 0;
		}
	}

	return obst;
}

struct MutationStrategy
{
	MutationStrategy(int type, std::size_t start = 0, std::size_t stop = 0)
	: type(type)
	, start(start)
	, stop(stop)
	{
	}

	int type;
	std::size_t start;
	std::size_t stop;
};

InputState generate(FollowAI& ai, Rand& rand, InputContext& prev)
{
	return ai.model.random(prev, rand);
}

double sigmoid(double x)
{
	return 1.0 / (1.0 + exp(-x));
}

// psigmoid(0) = 0
// psigmoid(1) ~= 0.5
// psigmoid(inf) = 1
double psigmoid(double x)
{
	return (sigmoid(x) - 0.5) * 2.0;
}

level_cell* AiContext::pathFind(int x, int y)
{
	auto* cell = dlevel.cell_from_px(x, y);
	bool path = dlevel.run(
		[=] { return cell->state == path_node::closed; },
		level_cell_succ());
	return path ? cell : 0;
}

struct Weights
{
	Weights()
	: healthWeight(1.0)
	, aimWeight(1.0)
	, distanceWeight(1.0)
	, ammoWeight(1.0)
	, missileWeight(1.0)
	{
	}

	double healthWeight, aimWeight, distanceWeight, ammoWeight, missileWeight;
};

double evaluateState(
	FollowAI& ai,
	Worm* me,
	Game& game,
	InputContext& context,
	Worm* target,
	Game& orgGame,
	int index)
{
	double score = 0;

	Weights weights;

	int posx = ftoi(me->x), posy = ftoi(me->y);

	auto* wormCell = ai.pathFind(posx, posy);

	double len = 200.0;
	if (wormCell)
	{
		double optimalDist = 10.0;
		double meNormHealth = orgGame.wormByIdx(me->index)->health * 100.0 / me->settings->health;
		double targetNormHealth = orgGame.wormByIdx(target->index)->health * 100.0 / target->settings->health;

		double ratio = (1 + targetNormHealth) / (1 + meNormHealth);

		if (game.settings->gameMode != Settings::GMHoldazone)
		{
			optimalDist = std::max(std::min(15.0 * ratio, 60.0), 10.0);
		}

		double d = std::abs(wormCell->g / 256.0 - optimalDist);
		len *= psigmoid(d / 100.0);
	}

	if (me->steerableCount == 1)
	{
		auto* missileCell = ai.dlevel.cell_from_px(me->steerableSumX, me->steerableSumY);
		bool missilePath = ai.dlevel.run(
			[=] { return missileCell->state == path_node::closed; },
			level_cell_succ());

		if (missilePath && wormCell && missileCell->g < wormCell->g)
		{
			double closer = (double)wormCell->g - missileCell->g;
			score += psigmoid((closer / (double)wormCell->g)) * 100.0 * weights.missileWeight;
		}
	}
	
	double meHealth = totalHealthNorm(me);
	double targetHealth = totalHealthNorm(target);

	score += meHealth * weights.healthWeight * 1.3;
	score -= targetHealth * weights.healthWeight;

	if (game.settings->gameMode == Settings::GMHoldazone
	 && game.holdazone.holderIdx == me->index)
	{
		double aimDiff = aimingDiff(me, target);
		score -= aimDiff * 2.0 * weights.aimWeight;
	}
	else
	{
		double aimDiff = aimingDiff(ai, me, wormCell);
		score -= aimDiff * 2.0 * weights.aimWeight;
	}

	{
		double meAmmoWorth = totalAmmoWorth(game, me);

		score += meAmmoWorth * 2.0 * weights.ammoWeight;
	}
	
	if (game.settings->gameMode == Settings::GMHoldazone)
	{
		double scale = 1.0/4.0;
		if (game.holdazone.holderIdx != me->index)
			scale = 1.0;

		score -= len * weights.distanceWeight * scale;
	}
	else if (game.settings->gameMode == Settings::GMGameOfTag)
	{
		if (game.lastKilledIdx >= 0 && game.lastKilledIdx != me->index)
			score += len * weights.distanceWeight;
		else
			score -= len * weights.distanceWeight;
	}
	else
	{
		score -= len * weights.distanceWeight;
	}

	if (!me->visible && !me->ready)
	{
	}
	else if (me->visible)
	{
#if 0
		double dx = (me->x - target->x) / 65536.0;
		double dy = (me->y - target->y) / 65536.0;

		double px = -dy;
		double py = dx;

		double vx = (me->velX / 65536.0);
		double vy = (me->velY / 65536.0);

		double d = std::sqrt(dx*dx + dy*dy);

		double s = (vx*px + vy*py) / d;

		score += std::min(std::abs(s), 5.0);
#endif
	}


	auto& c = ai.cell(me->x, me->y);
#if 0
	if (ai.maxPresence > 0.1)
		score -= (c.presence * 4.0) / ai.maxPresence;
	if (ai.maxDamage > 0.1)
			score -= (c.damage * 10.0) / ai.maxDamage;
#endif
	
	if (game.settings->gameMode == Settings::GMHoldazone)
	{
		if (game.holdazone.holderIdx == me->index)
			score += 50.0;
		if (game.holdazone.contenderIdx == me->index)
			score += game.holdazone.contenderFrames * 30.0 / 70.0;
			
		if (game.holdazone.holderIdx == target->index)
			score -= 50.0;
		if (game.holdazone.contenderIdx == target->index)
			score -= game.holdazone.contenderFrames * 30.0 / 70.0;
	}

	return score;
}

double EvaluateResult::weightedScore()
{
	double r = 0.0;
	double fallOff = 0.995;

	for (std::size_t i = 1; i < scoreOverTime.size(); ++i)
	{
		double weight = double(scoreOverTime.size() - i) / scoreOverTime.size();

		double diff = scoreOverTime[i] * weight;
		r += diff;
	}

	return r + futureScore;
}

void SimpleAI::process(Game& game, Worm& worm)
{
	Worm* target = game.wormByIdx(worm.index ^ 1);

	auto cs = worm.controlStates;

	if (!worm.visible)
	{
		cs.set(Worm::Fire, true);
	}
	else
	{
		int aim = normalizedLangle(worm.aimingAngle);
		double currentAim = langleToRadians(aim);

		double dirx = std::abs(target->x - worm.x);
		double diry = target->y - worm.y;
		double angleToTarget = std::atan2(diry, dirx);

		double tolerance = 2 * pi / 32.0;

		double aimDiff = radianDiff(angleToTarget, currentAim);

		bool fire = aimDiff >= -tolerance
			&& aimDiff <= tolerance
			&& obstacles(game, &worm, target) < 4;

#if 0
		if (fire && worm.weapons[worm.currentWeapon].loadingLeft > 0)
		{
			cs.set(Worm::Change, true);
			cs.set(Worm::Up, false);
			cs.set(Worm::Down, false);
			cs.set(Worm::Fire, false);
			cs.set(Worm::Left, true);
			cs.set(Worm::Right, false);
		}
		else
#endif
		{
			cs = initial;
			cs.set(Worm::Down, aimDiff < -tolerance);
			cs.set(Worm::Up, aimDiff > tolerance);
			cs.set(Worm::Fire, fire || initial[Worm::Fire]);
			cs.set(Worm::Change, false);

			if (cs[Worm::Fire] && target->x < worm.x && worm.direction != 0)
			{
				cs.set(Worm::Left, true);
				cs.set(Worm::Right, false);
			}
			else if (cs[Worm::Fire] && target->x > worm.x && worm.direction != 1)
			{
				cs.set(Worm::Left, false);
				cs.set(Worm::Right, true);
			}
		}
	}

	worm.controlStates = cs;
}

EvaluateResult evaluate(
	FollowAI& ai,
	Worm* me,
	Game& game,
	Worm* target,
	Plan& plan,
	std::size_t planSize,
	MutationStrategy const& ms)
{
	Game copy(game);
	copy.postClone(game);
	copy.quickSim = true;

	Worm* meCopy = copy.wormByIdx(me->index);
	Worm* targetCopy = copy.wormByIdx(target->index);

	FollowAI* targetAi = 0;

#if 1
	targetAi = dynamic_cast<FollowAI*>(target->ai.get());
#endif

	auto context = ai.currentContext;
	InputContext targetContext;
	if (targetAi)
		targetContext = targetAi->currentContext;

	EvaluateResult result;
	SimpleAI simpleAi;

	simpleAi.initial = targetCopy->controlStates;

	result.scoreOverTime.push_back(evaluateState(ai, meCopy, copy, context, targetCopy, game, 0));

	for (std::size_t i = 0; i < planSize; ++i)
	{
		if (plan.size() <= i)
		{
			plan.push_back(generate(ai, ai.rand, context));
		}

		if (ms.type == 0)
		{
			// Do nothing
		}
		else if (ms.type == 1)
		{
			if (i >= ms.start && i < ms.stop)
			{
				plan[i] = generate(ai, ai.rand, context);
			}
		}
		else if (ms.type == 2)
		{
			if (i >= ms.start && i < ms.stop)
			{
				auto old = plan[i];
				do
					plan[i] = generate(ai, ai.rand, context);
				while (plan[i].idx == old.idx);
			}
		}

		meCopy->controlStates = context.update(plan[i], copy, meCopy, ai);

		if (targetAi && i < targetAi->plan.size())
			targetCopy->controlStates = targetContext.update(targetAi->plan[i], copy, targetCopy, *targetAi);
		else
			simpleAi.process(copy, *targetCopy);

		copy.processFrame();

		double s = evaluateState(ai, meCopy, copy, context, targetCopy, game, i + 1);

		result.scoreOverTime.push_back(s);
	}
	
	double prev = result.scoreOverTime[0];
	result.scoreOverTime[0] = 0.0;
	for (std::size_t i = 1; i < result.scoreOverTime.size(); ++i)
	{
		double score = result.scoreOverTime[i];
		result.scoreOverTime[i] = score - prev;
		prev = score;
	}

	return std::move(result);
}

EvaluateResult mutate(
	FollowAI& ai,
	Game& game, Worm& worm, Worm* target, Plan& candidate,
	int i, EvaluateResult const& prevResult)
{
	//auto candidate = best;

	MutationStrategy ms(1, 0, candidate.size());

	if (i == 0)
	{
		ms.type = 0;
	}
#if 0
	else if (i == 1)
	{
		ms.type = 2;
		ms.start = 10;
		ms.stop = ms.start + 1;
	}
#endif
	else
	{
		// Find the minimum suffix sum
		std::size_t j = prevResult.scoreOverTime.size() - 1;

		double sum = prevResult.scoreOverTime[j];
		double min = sum;
		auto minj = j;

		while (j-- > 0)
		{
			sum += prevResult.scoreOverTime[j];
			if (sum < min)
			{
				minj = j;
				min = sum;
			}
		}

		if (ai.rand(8) < 7)
		{
			ms.stop = ai.rand(minj);
			ms.start = ai.rand(std::max(ms.stop, std::size_t(10)) - 10, ms.stop);
		}
		else
		{
			ms.start = ai.rand(0, candidate.size());
			ms.stop = ai.rand(ms.start, std::min(ms.start + 10, candidate.size()));
		}
	}
		
	EvaluateResult result(evaluate(ai, &worm, game, target, candidate, game.settings->aiFrames, ms));

#if 0
	if (ms.type == 2
	&& prevResult.scoreOverTime.size() == result.scoreOverTime.size())
	{
		ai.negEffect.resize(prevResult.scoreOverTime.size());
		ai.posEffect.resize(prevResult.scoreOverTime.size());

		for (std::size_t i = 0; i < prevResult.scoreOverTime.size(); ++i)
		{
			double diff = result.scoreOverTime[i] - prevResult.scoreOverTime[i];
			if (diff < 0.0)
				ai.negEffect[i] += -diff;
			else
				ai.posEffect[i] += diff;
		}
		++ai.effectScaler;

		if ((game.cycles % 70) == 1)
		{
			for (std::size_t i = 8; i < 8 + 15; ++i)
			{
				printf("%4.02f ", ai.posEffect[i] / ai.effectScaler);
			}
			printf("\n");
		}
	}
#endif

	return std::move(result);
}

void FollowAI::drawDebug(Game& game, Worm const& worm, Renderer& renderer, int offsX, int offsY)
{

#if 0
	auto* cell = pathFind(ftoi(worm.x), ftoi(worm.y));
#endif

#if 0
	{
		AiContext& context = *this;
		Worm const* from = &worm;
		if (!cell || !cell->parent)
			return;
		auto org = context.dlevel.coords(cell);

		double dirx = 0, diry = 0;
		auto* c = cell;
		int count = 10;
		while (count-- > 0)
		{
			c = (level_cell*)c->parent;
			if (!c)
				break;

			auto path = context.dlevel.coords(c);

			int xdiff = std::abs(path.first - org.first);
			int ydiff = path.second - org.second;

			double len = std::sqrt(xdiff*xdiff + ydiff*ydiff);
			dirx += xdiff / len;
			diry += ydiff / len;
		}

		int aim = normalizedLangle(from->aimingAngle);
		double currentAim = langleToRadians(aim);

		double angleToTarget = std::atan2(diry, dirx);

		double tolerance = pi / 8;
		double aimDiff = std::abs(radianDiff(angleToTarget, currentAim));

		//double d = std::floor(aimDiff / tolerance) / 6.0;
		double d = std::max(aimDiff - tolerance, 0.0) / 6.0;

		std::ostringstream s; s << d;

		game.common->font.drawFramedText(renderer.screenBmp, s.str(), 10, 10, 7);

		drawLine(renderer.screenBmp,
			ftoi(worm.x) + offsX, ftoi(worm.y) + offsY,
			ftoi(worm.x) + offsX + (int)(std::cos(angleToTarget) * 20.0),
			ftoi(worm.y) + offsY + (int)(std::sin(angleToTarget) * 20.0),
			7);
			/*
		drawLine(renderer.screenBmp,
			ftoi(worm.x) + offsX, ftoi(worm.y) + offsY,
			ftoi(worm.x) + offsX + (int)(std::cos(currentAim) * 10.0),
			ftoi(worm.y) + offsY + (int)(std::sin(currentAim) * 10.0),
			3);*/
	}
#endif

#if 0
	//auto* cell = aiContext.dlevel.cell_from_px(ftoi(worm.x), ftoi(worm.y));
	
	while (cell)
	{
		auto coords = dlevel.coords(cell);
		fillRect(renderer.screenBmp,
			coords.first * dlevel.factor + offsX,
			coords.second * dlevel.factor + offsY,
			dlevel.factor,
			dlevel.factor,
			3);
		cell = (level_cell*)cell->parent;
	}
#endif
}

void FollowAI::process(Game& game, Worm& worm)
{
	Common& common = *game.common;

	Worm* target = game.worms[worm.index ^ 1];

	{
		int targetx = ftoi(target->x), targety = ftoi(target->y);

		if (game.settings->gameMode == Settings::GMHoldazone)
		{
			targetx = game.holdazone.rect.center_x();
			targety = game.holdazone.rect.center_y();
		}

		dlevel.build(game.level, common);
		auto* targetCell = dlevel.cell_from_px(targetx, targety);
		targetCell->cost = 1;
		dlevel.set_origin(targetCell);
	}

#if 0
	aiContext.dlevel.run(
		aiContext.dlevel.cell_from_px(ftoi(worm.x), ftoi(worm.y)),
		level_cell_succ());

	if ((game.cycles % 70) == 0)
	{
		printf("%d\n", aiContext.dlevel.cell_from_px(ftoi(worm.x), ftoi(worm.y))->g);
	}
#endif

	update(*this, worm);
	if (targetAi)
		targetAi->update(*targetAi, *target);

#if NDEBUG
	if ((frame % 1) == 0)
#else
	if ((frame % 20) == 0)
#endif
	{
		Plan best = plan;

		double bestScore = -DBL_MAX;

		int evaluations = 0;

		if (prevResultAge < 3
		&& !prevResult.scoreOverTime.empty())
		{
			// Shift result
			prevResult.scoreOverTime.erase(prevResult.scoreOverTime.begin());
			prevResult.scoreOverTime.push_back(0.0);

			bestScore = prevResult.weightedScore();
			++prevResultAge;
		}
		else
		{
			EvaluateResult result(mutate(*this, game, worm, target, best, 0, prevResult));
			++evaluations;

			bestScore = result.weightedScore();
			prevResult = std::move(result);
			prevResultAge = 0;
		}

		while (evaluations < game.settings->aiMutations + 1)
		{
			auto candidate = best;
			EvaluateResult result(mutate(*this, game, worm, target, candidate, 1, prevResult));
			++evaluations;

			double weightedScore = result.weightedScore();
			if (weightedScore >= bestScore)
			{
				best = candidate;
				bestScore = weightedScore;
				prevResult = std::move(result);
				prevResultAge = 0;
			}
		}
		
		plan = best;
	}

	worm.controlStates = currentContext.update(plan[0], game, &worm, *this);
	
	plan.erase(plan.begin());

	if (targetAi)
	{
		targetAi->currentContext.update(targetAi->plan[0], game, target, *targetAi);
		targetAi->plan.erase(targetAi->plan.begin());
	}

	++frame;
	if (targetAi)
		++targetAi->frame;
}

Worm::ControlState InputContext::update(InputState newState, Game& game, Worm* worm, AiContext& aiContext)
{
	Worm::ControlState cs;

	if (worm->visible && wantedWeapon != worm->currentWeapon)
	{
		int offset = wantedWeapon - worm->currentWeapon;
		offset = ((offset + 2 + 5) % 5) - 2;
		cs.set(Worm::Left, offset < 0);
		cs.set(Worm::Right, offset > 0);
		cs.set(Worm::Change, true);
	}
	else
	{
		int pa, pb, pc;
		switch (newState.decompose(pa, pb, pc))
		{
			case InputState::MoveJumpFire:
			{
				cs.set(Worm::Up, (pa >> 1) & 1);
				cs.set(Worm::Down, pa & 1);
				cs.set(Worm::Left, (pb >> 1) & 1);
				cs.set(Worm::Right, pb & 1);
				cs.set(Worm::Fire, (pc >> 1) & 1);
				cs.set(Worm::Jump, pc & 1);
				break;
			}

			case InputState::ChangeWeapon:
			{
				if (wantedWeapon != pa)
				{
					wantedWeapon = pa;
					cs.set(Worm::Change, true);
				}
				break;
			}

			case InputState::RopeUpDown:
			{
				cs.set(Worm::Up, (pa >> 1) & 1);
				cs.set(Worm::Down, pa & 1);
				cs.set(Worm::Change, 1);
				if (pa == 0)
					cs.set(Worm::Jump, 1);
				break;
			}
		}
	}

	currentState = newState;

	if (!worm->visible && !worm->ready)
		++hiddenFrames;
	else
		hiddenFrames = 0;

	facingEnemy = (game.worms[worm->index ^ 1]->x > worm->x) == worm->direction;
	ninjaropeOut = worm->ninjarope.out;

	return cs;
}

void transToM(double& p,
	int pa, int pb, int pc, int facingEnemy, int ninjaropeOut,
	int pa2, int pb2, int pc2)
{
	// P(pa -> pa2) P(pb -> pb2) P(pc -> pc2)

	assert(pa < 3 && pb < 4 && pc < 4);
	assert(pa2 < 3 && pb2 < 4 && pc2 < 4);
	assert(facingEnemy < 2 && ninjaropeOut < 2);
		
	if (pa == 0) p *= select(pa2, 0.1,   0.45,  0.45);  // Start aiming
	if (pa == 1) p *= select(pa2, 0.025, 0.9,   0.075); // Aiming down
	if (pa == 2) p *= select(pa2, 0.025, 0.075, 0.9);   // Aiming up

	if (pb == 0) p *= select(pb2, 0.05,  0.4,  0.4,  0.15);  // Not moving

	double swapBias = 0.0; // Probability that direction would switch without prompting
	if (pb == 1) p *= select(pb2, 0.005, 0.965,  0.005,  0.025); // Moving right
	if (pb == 2) p *= select(pb2, 0.005, 0.005,  0.965,  0.025); // Moving left
	if (pb == 3) p *= select(pb2, 0.1,   0.35, 0.35, 0.2);   // Digging

	// Fire
	double startShootP = facingEnemy ? 0.15 : 0.03;
	p *= select((pc & 2)        | ((pc2 & 2) >> 1),
		1.0 - startShootP, startShootP,
		0.4, 0.6);

	// Jump
	double startJump = ninjaropeOut ? 0.015 : 0.1;
	p *= select(((pc & 1) << 1) | (pc2 & 1),
		1.0 - startJump, startJump, // 0 -> 0, 0 -> 1
		0.999, 0.001);              // 1 -> 0, 1 -> 1
}

TransModel::TransModel(bool testing)
{
	for (int i = 0; i < this->states; ++i)
	{
		int facingEnemy, ninjaropeOut;
		auto prev = InputContext::unpack(i, facingEnemy, ninjaropeOut);
		int pa, pb, pc;
		auto type = prev.decompose(pa, pb, pc);

		double sum2 = 0.0;

		for (int j = 0; j < this->freeStates; ++j)
		{
			int pa2, pb2, pc2;
			auto type2 = InputState(j).decompose(pa2, pb2, pc2);

			double p = 1;

			if (type == InputState::MoveJumpFire)
			{
				double m = 0.95, c = 0.03, r = 0.02;

#if 0 // Doesn't seem to help
				if (testing)
				{
					if (pc & 2) // Firing, larger chance of changing weapon
					{
						c += 0.07;
						m -= 0.07;
					}
				}
#endif

				if (type2 == InputState::MoveJumpFire)
				{
					p *= m;
					transToM(p, pa, pb, pc, facingEnemy, ninjaropeOut, pa2, pb2, pc2);
				}
				else if (type2 == InputState::ChangeWeapon)
				{
					p *= c;
					p *= 1.0 / 5.0; // Each weapon has equal probability
				}
				else if (type2 == InputState::RopeUpDown)
				{
					p *= r;
					p *= select(pa2, 0.98, 0.01, 0.01); // TODO: Rope out
				}
			}
			else if (type == InputState::ChangeWeapon)
			{
				if (type2 == InputState::MoveJumpFire)
				{
					// Same as from the idle m-state
					p *= 0.97;
					transToM(p, 0, 0, 0, facingEnemy, ninjaropeOut, pa2, pb2, pc2);
				}
				else if (type2 == InputState::ChangeWeapon)
				{
					p *= 0.001; // Very little chance
					p *= 1.0 / 5.0; // Each weapon has equal probability
				}
				else if (type2 == InputState::RopeUpDown)
				{
					p *= 0.029;
					p *= select(pa2, 0.98, 0.01, 0.01); // TODO: Rope out
				}
			}
			else if (type == InputState::RopeUpDown)
			{
				if (type2 == InputState::MoveJumpFire)
				{
					// Same as from the idle state
					p *= 0.95;
					transToM(p, 0, 0, 0, facingEnemy, ninjaropeOut, pa2, pb2, pc2);
				}
				else if (type2 == InputState::ChangeWeapon)
				{
					p *= 0.049;
					p *= 1.0 / 5.0; // Each weapon has equal probability
				}
				else if (type2 == InputState::RopeUpDown)
				{
					p *= 0.001; // Very little chance
					p *= select(pa2, 0.98, 0.01, 0.01); // TODO: Rope out
				}
			}

			sum2 += p;
				
			trans[i][j] = p;
		}

		auto& v = trans[i];
		double sum = std::accumulate(v, v + this->freeStates, 0.0);

		if (sum < 0.999 || sum > 1.0001)
			printf("%d: %f\n", i, sum);
	}
}

void AiContext::update(FollowAI& ai, Worm& worm)
{
	//auto& p = cell(ai.worm.x, ai.worm.y);
	double presence = 0, damage = 0;

	if (worm.visible)
	{
		//p.presence = std::min(p.presence + 4.0/(70.0 * 5.0), 4.0);
		presence = 4.0/(70.0 * 5.0);
	}

	double hp = totalHealthNorm(&worm);

	if (hp < prevHp)
	{
		//p.damage += std::min((prevHp - hp) / 10.0, 10.0);

		damage = (prevHp - hp);
	}

	incArea(worm.x, worm.y, presence, damage);

#if 1
	for (int y = 0; y < height; ++y)
	for (int x = 0; x < width; ++x)
	{
		state[x][y].presence = std::max(state[x][y].presence - 0.5/(70.0 * 5.0), 0.0);
	}
#endif
}