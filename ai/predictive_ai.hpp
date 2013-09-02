#ifndef LIERO_PREDICTIVE_AI_HPP
#define LIERO_PREDICTIVE_AI_HPP

#include "../worm.hpp"
#include "../math.hpp"
#include "../rand.hpp"
#include <numeric>
#include <functional>
#include <algorithm>
#include "dijkstra.hpp"
#include "work_queue.hpp"
#include <SDL/SDL.h>

struct InputState
{
	enum Type
	{
		MoveJumpFire = 0, // 48, aabb0cc, a != 3
		ChangeWeapon = 1, // 5,  aaa, 0..4, translated to 0010100 or 0001100
		RopeUpDown = 2,   // 3,  aa00110, a != 3
	};

	InputState(Worm* w)
	{
		auto cs = w->controlStates;
		auto v = cs.pack();

		if (!cs[Worm::Change])
		{
			// MoveJumpFire
			idx = (v >> 2) << 1;
			if (idx > 48) idx -= (1 << 5);
				idx = 2;
			idx |= (v & 1);
		}
		else
		{
			if (!cs[Worm::Jump])
			{
				// ChangeWeapon
				idx = 48 + w->currentWeapon;
			}
			else
			{
				// RopeUpDown
				idx = 48 + 5 + (v >> 5);
			}
		}
	}

	InputState(int idx = 0)
	: idx(idx)
	{
	}

	int idx; // 0..56

	Type decompose(int& pa, int& pb, int& pc)
	{
		int i = idx;
		if (i < 48)
		{
			pa = i >> 4;
			pb = (i >> 2) & 3;
			pc = i & 3;
			return MoveJumpFire;
		}
		i -= 48;

		if (i < 5)
		{
			pa = i;
			return ChangeWeapon;
		}
		i -= 5;

		if (i < 3)
		{
			pa = i;
			return RopeUpDown;
		}

		assert(false);
		return MoveJumpFire;
	}

	static InputState compose(Type type, int pa, int pb, int pc)
	{
		int idx = 0;
		switch (type)
		{
			case MoveJumpFire:
			{
				idx = (pa << 4) | (pb << 2) | pc;
				break;
			}

			case ChangeWeapon:
			{
				idx = 48 + pa;
				break;
			}

			case RopeUpDown:
			{
				idx = 48 + 5 + pa;
				break;
			}
		}

		return InputState(idx);
	}

	
};

typedef std::vector<InputState> Plan;

template<typename T>
inline T select(int n, T first)
{
	assert(n == 0);
	return first;
}

template<typename T>
inline T select(int n, T first, T a)
{
	if (n == 0) return first;
	return select(n - 1, a);
}

template<typename T>
inline T select(int n, T first, T a, T b)
{
	if (n == 0) return first;
	return select(n - 1, a, b);
}

template<typename T>
inline T select(int n, T first, T a, T b, T c)
{
	if (n == 0) return first;
	return select(n - 1, a, b, c);
}
/*
template<typename T>
inline T select(int n, T first, T a, T b, T c, T d)
{
	if (n == 0) return first;
	return select(n - 1, a, b, c, d);
}*/

struct AiContext;

struct InputContext
{
	InputContext()
	: wantedWeapon(0)
	, hiddenFrames(0)
	, facingEnemy(0)
	, ninjaropeOut(0)
	{
	}

	Worm::ControlState update(InputState newState, Game& game, Worm* worm, AiContext& aiContext);

	int pack()
	{
		int i = ninjaropeOut;
		i = i*2 + facingEnemy;
		i = i*56 + currentState.idx;
		return i;
	}

	static InputState unpack(int idx, int& facingEnemy, int& ninjaropeOut)
	{
		int s = idx % 56;
		idx /= 56;
		facingEnemy = idx % 2;
		idx /= 2;
		ninjaropeOut = idx;
		return InputState(s);
	}

	static int const Size = 56 * 2 * 2;

	// Free part
	InputState currentState;

	// Dependent part
	int wantedWeapon;
	int hiddenFrames;
	int facingEnemy;
	int ninjaropeOut;
};

template<int States, int FreeStates>
struct Model
{
	static int const states = States;
	static int const freeStates = FreeStates;
	double trans[States][FreeStates];

	int random(int context, Rand& rand)
	{
		assert(context < States);
		auto& v = trans[context];

		double max = std::accumulate(v, v + FreeStates, 0.0);
		double el = rand.get_double(max);

		for (int i = 0; i < FreeStates; ++i)
		{
			el -= v[i];
			if (el < 0.0)
				return i;
		}

		return FreeStates - 1;
	}
};

struct Weights
{
	Weights()
	: healthWeight(1.0)
	, aimWeight(1.0)
	, distanceWeight(1.0)
	, ammoWeight(1.0)
	, missileWeight(1.0)
	, defenseWeight(1.3)
	, firingWeight(1.0)
	{
	}

	double healthWeight, aimWeight, distanceWeight, ammoWeight, missileWeight;
	double defenseWeight, firingWeight;
};

struct TransModel : Model<InputContext::Size, 56>
{

	TransModel(Weights& weights, bool testing);

#if 1
	void update(InputContext context, InputState v)
	{
		trans[context.pack()][v.idx] += 0.0005;
	}
#endif

	InputState random(InputContext context, Rand& rand)
	{
		return InputState(Model<InputContext::Size, 56>::random(context.pack(), rand));
	}
};

struct CellState
{
	double presence;
	double damage; // Health decrease in this cell
};

struct FollowAI;

struct AiContext
{
	static int const width = (504 + 31) >> 5;
	static int const height = (350 + 31) >> 5;

	AiContext()
	: prevHp(0)
	, maxDamage(0)
	, maxPresence(0)
	{
	}
	
	dijkstra_level dlevel;

	CellState state[width][height];

	int prevHp;
	double maxDamage, maxPresence;

	void incArea(int fx, int fy, double presence, double damage)
	{
		int wx = ftoi(fx) >> 5;
		int wy = ftoi(fy) >> 5;

		for (int y = wy - 1; y <= wy + 1; ++y)
		for (int x = wx - 1; x <= wx + 1; ++x)
		{
			if (y >= 0 && y < height && x >= 0 && x < width)
			{
				double d = 1.0;
				if (x != wx) d *= 0.5;
				if (y != wy) d *= 0.5;
				auto& c = state[x][y];
				c.presence = d * presence;
				c.damage += d * damage;
				maxDamage = std::max(maxDamage, c.damage);
				maxPresence = std::max(maxPresence, c.presence);
			}
		}
	}

	CellState& cell(int fx, int fy)
	{
		int wx = ftoi(fx) >> 5;
		int wy = ftoi(fy) >> 5;

		wx = std::max(std::min(wx, width), 0);
		wy = std::max(std::min(wy, height), 0);

		return state[wx][wy];
	}

	void update(FollowAI& ai, Worm& worm);
	level_cell* pathFind(int x, int y);
};

struct EvaluateResult
{
	EvaluateResult()
	: futureScore(0.0)
	{
	}

	double weightedScore();

	std::vector<double> scoreOverTime;
	double futureScore;
};

struct SimpleAI : WormAI
{
	void process(Game& game, Worm& worm);

	Worm::ControlState initial;
};

struct AIThread
{
	AIThread()
	: th(0)
	{
	}

	SDL_Thread* th;
};

struct FollowAI : WormAI, AiContext
{
	FollowAI(Weights weights, bool testing, FollowAI* targetAiInit = 0)
	: frame(0)
	, prevResultAge(0)
	, model(weights, testing)
	, targetAi(targetAiInit)
	, effectScaler(0)
	, weights(weights)
#if AI_THREADS
	, workQueue(2)
#endif
	{
#if 0
		if (!targetAi)
			targetAi = new FollowAI(this);
#endif
	}

	~FollowAI()
	{
	}

	void process(Game& game, Worm& worm);

	void drawDebug(Game& game, Worm const& worm, Renderer& renderer, int offsX, int offsY);
	
	Rand rand;
	int frame;
	Plan plan;
	EvaluateResult prevResult;
	int prevResultAge;
	InputContext currentContext;
	TransModel model;

	std::vector<double> negEffect, posEffect;
	int effectScaler;

	FollowAI* targetAi;

#if AI_THREADS
	WorkQueue workQueue;
#endif

	Weights weights;
};

#endif // LIERO_PREDICTIVE_AI_HPP
