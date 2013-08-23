#include <gvl/containers/pairing_heap.hpp>

#include "../level.hpp"
#include "../common.hpp"

struct path_node : gvl::pairing_node<path_node>
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
		return f < b.f;
	}
	
	void update_cost(int g_new)
	{
		g = g_new;
		f = g + h;
	}

	void reset()
	{
		state = none;
	}
	
	path_node* parent;
	int state;
	
	int f, g, h; // TODO: Generic type
};

template<typename NodeT, typename DerivedT>
struct astar_state
{
	typedef path_node path_node_t;

	// open_list contains elements of nodes, so it's important that open_list is last
	//vl::hash_set<path_node_t, node_hash<NodeT>, node_compare<NodeT> > nodes;
	gvl::pairing_heap<path_node_t, std::less<path_node_t>, gvl::dummy_delete> open_list;
	
	void add_open(path_node_t* node)
	{
		sassert(node->state != path_node_t::open);
		open_list.insert(node);
		node->state = path_node_t::open;
	}
	
	// get_node(NodeT) -> path_node_t*
	// get_node_id(path_node_t*) -> NodeT

	template<
		typename NodeT,
		typename Heuristic,
		typename ResultOutputIterator,
		typename SuccessorIter>
	bool a_star(NodeT origin, NodeT target, SuccessorIter succ_iter, ResultOutputIterator result, Heuristic heuristic)
	{
		typedef astar_state<NodeT> astar_state_t;
		typedef astar_state_t::path_node_t path_node_t;
		//astar_state_t state;

		DerivedT& derived = static_cast<DerivedT&>(*this);
	
		path_node_t* origin_pn = derived.get_node(origin);
		origin_pn->h = heuristic(origin, target);
		origin_pn->update_cost(0);
		state.add_open(origin_pn);
	
		while(!state.open_list.empty())
		{
			path_node_t* min_pn = state.open_list.unlink_min();
			NodeT min = derived.get_node_id(min_pn);
		
			if(min == target)
			{
				// Success, output result
			
				do
				{
					*result = derived.get_node_id(min_pn);
					++result;
					min_pn = min_pn->parent;
				}
				while(min_pn);
			
				return true;
			}
		
			min_pn->state = path_node_t::closed;
		
			succ_iter.begin(min);
		
			// TODO: We can avoid adding/updating one node if it's better than the
			// best node in open_list. We simply save it and pretend we got it
			// from the open_list in the next iteration. Is this useful? I suppose not.
		
			for(; succ_iter; ++succ_iter)
			{
				NodeT node = succ_iter.node();
				path_node_t* pn = derived.get_node(node);
				if(pn->state != path_node_t::closed)
				{
					int g = min_pn->g + succ_iter.cost();
					if(pn->state != path_node_t::open)
					{
						pn->h = heuristic(node, target);
						pn->parent = min_pn;
						pn->update_cost(g);
						state.add_open(pn);
					}
					else if(g < pn->g)
					{
						pn->update_cost(g);
						pn->parent = min_pn;
						state.open_list.decreased_key(pn);
					}
				}
			}
		}
	
		return false;
	}
};

struct level_cell : path_node
{
	 int cost; // 1 = air, 2 = dirt, -1 = rock
};

struct level_cell_succ
{
	void begin(level_cell* c)
	{
		i = 0;
		this->c = c;
	}
	
	void operator++()
	{
		++i;
	}
	
	operator bool() const
	{
		return i < 8;
	}
	
	int node()
	{
		
	}
	
	int cost()
	{
		return edges[i].cost;
	}
	
	level_cell* c;
	int i;
};

struct astar_level : astar_state<level_cell*, astar_level>
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

	void build(Level& level, Common& common)
	{
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

		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				int lx = x / factor;
				int ly = y / factor;

				int cost = 1;

				for (int cy = ly * factor; cy < (ly + 1) * factor; ++cy)
				for (int cx = lx * factor; cx < (lx + 1) * factor; ++cx)
				{
					Material m = mat[level.pixel(cx, cy)];

					if (m.rock())
					{
						cost = -1;
						goto done;
					}
					else if (m.worm())
					{
						cost = 2;
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