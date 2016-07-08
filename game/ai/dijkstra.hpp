#ifndef LIERO_AI_DIJKSTRA_HPP
#define LIERO_AI_DIJKSTRA_HPP

#include <gvl/containers/pairing_heap.hpp>
#include <gvl/math/vec.hpp>

#include "../level.hpp"
#include "../common.hpp"

struct path_node : gvl::pairing_node<>
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
	{
	}
	
	bool operator<(path_node const& b) const
	{
		return g < b.g;
	}
	
	void reset()
	{
		state = none;
	}
	
	path_node* parent;
	int state;
	
	int g; // TODO: Generic type
};

template<typename NodeT, typename DerivedT>
struct dijkstra_state
{
	typedef path_node path_node_t;

	// open_list contains elements of nodes, so it's important that open_list is last
	gvl::pairing_heap<path_node_t, gvl::default_pairing_tag, std::less<path_node_t>, gvl::dummy_delete> open_list;

	void add_open(path_node_t* node)
	{
		sassert(node->state != path_node_t::open);
		open_list.insert(node);
		node->state = path_node_t::open;
	}
	
	// get_node(NodeT) -> path_node_t*
	// get_node_id(path_node_t*) -> NodeT

	void reset()
	{
		open_list.unlink_all();
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
		typedef dijkstra_state<NodeT, DerivedT> dijkstra_state_t;
		typedef typename dijkstra_state_t::path_node_t path_node_t;

		DerivedT& derived = static_cast<DerivedT&>(*this);

		int steps = 0, expansions = 0, neighbours = 0;

		while(!open_list.empty() && !stop())
		{
			path_node_t* min_pn = open_list.unlink_min();
			NodeT min = derived.get_node_id(min_pn);

			++steps;
		
			min_pn->state = path_node_t::closed;

			succ_iter.begin(min);

			while (succ_iter.advance())
			{
				NodeT node = succ_iter.node();
				path_node_t* pn = derived.get_node(node);
				if(pn->state != path_node_t::closed)
				{
					++neighbours;
					int g = min_pn->g + succ_iter.cost();
					if(pn->state != path_node_t::open)
					{
						pn->g = g;
						pn->parent = min_pn;
						add_open(pn);
						++expansions;
					}
					else if(g < pn->g)
					{
						pn->g = g;
						pn->parent = min_pn;
						open_list.decreased_key(pn);
					}
				}
			}
		}

		//printf("failed after %d expansions, %d neighbours\n", expansions, neighbours);
	
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

	gvl::ivec2 coords(level_cell* c)
	{
		int offset = (int)(c - cells);
		int y = offset / pitch;
		int x = offset % pitch;
		return gvl::ivec2(x - 1, y - 1);
	}

	gvl::ivec2 coords_level(level_cell* c)
	{
		return coords(c) * factor + gvl::ivec2(factor / 2, factor / 2);
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

#endif // LIERO_AI_DIJKSTRA_HPP
