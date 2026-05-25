#pragma once

#include <queue>
#include <utility>
#include <vector>

#include "math/rect.hpp"

#include "../level.hpp"
#include "../common.hpp"

struct path_node
{
	enum
	{
		none,
		open,
		closed
	};

	path_node()
	: parent(0)
	, state(none)
	, g(0)
	{
	}

	void reset()
	{
		state = none;
		parent = 0;
	}

	path_node* parent;
	int state;
	int g;
};

template<typename NodeT, typename DerivedT>
struct dijkstra_state
{
	typedef path_node path_node_t;

	// Lazy-deletion min-heap. When a node's cost is lowered, we push a new
	// entry; the stale entry remains in the heap and is skipped at pop time
	// (its cached `g` won't match the node's current `g`).
	struct Entry { int g; path_node_t* node; };
	struct EntryGreater { bool operator()(Entry const& a, Entry const& b) const { return a.g > b.g; } };
	std::priority_queue<Entry, std::vector<Entry>, EntryGreater> open_list;

	bool open_empty() const { return open_list.empty(); }

	void add_open(path_node_t* node)
	{
		open_list.push({node->g, node});
		node->state = path_node_t::open;
	}

	void reset()
	{
		while (!open_list.empty()) open_list.pop();
	}

	void set_origin(NodeT origin_init)
	{
		DerivedT& derived = static_cast<DerivedT&>(*this);

		path_node_t* origin_pn = derived.get_node(origin_init);
		origin_pn->g = 0;
		origin_pn->parent = 0;
		add_open(origin_pn);
	}

	template<
		typename StopPred,
		typename SuccessorIter>
	bool run(StopPred stop, SuccessorIter succ_iter)
	{
		DerivedT& derived = static_cast<DerivedT&>(*this);

		while(!open_list.empty() && !stop())
		{
			Entry e = open_list.top();
			open_list.pop();

			// Skip stale entries from prior decreased_key operations.
			if (e.node->state == path_node_t::closed) continue;
			if (e.g != e.node->g) continue;

			path_node_t* min_pn = e.node;
			NodeT min = derived.get_node_id(min_pn);

			min_pn->state = path_node_t::closed;

			succ_iter.begin(min);

			while (succ_iter.advance())
			{
				NodeT node = succ_iter.node();
				path_node_t* pn = derived.get_node(node);
				if(pn->state != path_node_t::closed)
				{
					int g = min_pn->g + succ_iter.cost();
					if(pn->state != path_node_t::open)
					{
						pn->g = g;
						pn->parent = min_pn;
						add_open(pn);
					}
					else if(g < pn->g)
					{
						pn->g = g;
						pn->parent = min_pn;
						// Push a new entry; the older one will be ignored at pop.
						open_list.push({g, pn});
					}
				}
			}
		}

		return stop();
	}
};

struct level_cell : path_node
{
	int cost; // 1 = air, 2 = dirt, -1 = rock
};

extern int const level_cell_offsets[8];
extern int const level_cell_costs[8];

struct level_cell_succ
{
	void begin(level_cell* c)
	{
		i = -1;
		this->c = c;
	}

	bool advance()
	{
		do
		{
			++i;
			if (i >= 8) return false;
		}
		while (cost() < 0);

		return true;
	}

	level_cell* node()
	{
		return &c[level_cell_offsets[i]];
	}

	int cost()
	{
		level_cell* n = c + level_cell_offsets[i];
		return level_cell_costs[i] * n->cost;
	}

	level_cell* c;
	level_cell* target;
	int i;
};

struct dijkstra_level : dijkstra_state<level_cell*, dijkstra_level>
{
	static int const full_width = 504;
	static int const full_height = 350;

	static int const factor = 4;

	static int const width = full_width / factor;
	static int const pitch = width + 2;
	static int const height = full_height / factor;

	level_cell cells[(width + 2) * (height + 2)];

	path_node_t* get_node(level_cell* c)
	{
		return c;
	}

	level_cell* get_node_id(path_node_t* c)
	{
		return static_cast<level_cell*>(c);
	}

	level_cell* cell(int x, int y)
	{
		return &cells[(y + 1) * pitch + x + 1];
	}

	IVec2 coords(level_cell* c)
	{
		int offset = (int)(c - cells);
		int y = offset / pitch;
		int x = offset % pitch;
		return IVec2(x - 1, y - 1);
	}

	IVec2 coords_level(level_cell* c)
	{
		return coords(c) * factor + IVec2(factor / 2, factor / 2);
	}

	level_cell* cell_from_px(int x, int y)
	{
		x = std::min(std::max(x, 0), full_width - 1);
		y = std::min(std::max(y, 0), full_height - 1);
		x /= factor;
		y /= factor;
		return cell(x, y);
	}

	void build(Level& level, Common& common)
	{
		this->reset();

		Material* mat = common.materials;

		for (int x = 0; x < width + 2; ++x)
		{
			cells[x].cost = -1;
			cells[(height + 1)*pitch + x].cost = -1;
		}

		for (int y = 0; y < height + 2; ++y)
		{
			cells[y*pitch].cost = -1;
			cells[y*pitch + (width + 1)].cost = -1;
		}

		for (int ly = 0; ly < height; ++ly)
		{
			for (int lx = 0; lx < width; ++lx)
			{
				int cost = 1;

				for (int cy = ly * factor; cy < (ly + 1) * factor; ++cy)
				for (int cx = lx * factor; cx < (lx + 1) * factor; ++cx)
				{
					if (!level.inside(cx, cy))
					{
						cost = -1;
						goto done;
					}

					Material m = mat[level.pixel(cx, cy)];

					if (!m.background())
					{
						if (m.anyDirt())
						{
							cost = 2;
						}
						else
						{
							cost = -1;
							goto done;
						}
					}
				}
			done:

				level_cell* c = cell(lx, ly);

				c->cost = cost;
			}
		}

		for (auto& c : cells)
			c.reset();
	}
};
