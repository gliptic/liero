#include "dijkstra.hpp"

int const level_cell_offsets[8] = {
	-1 * dijkstra_level::pitch + 0,
	 1 * dijkstra_level::pitch + 0,
	 0 * dijkstra_level::pitch - 1,
	 0 * dijkstra_level::pitch + 1,

	-1 * dijkstra_level::pitch - 1,
	-1 * dijkstra_level::pitch + 1,
	 1 * dijkstra_level::pitch - 1,
	 1 * dijkstra_level::pitch + 1
};

int const level_cell_costs[8] = {
	256,
	256,
	256,
	256,

	362,
	362,
	362,
	362
};