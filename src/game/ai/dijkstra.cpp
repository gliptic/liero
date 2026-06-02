#include "dijkstra.hpp"

int const kLevelCellOffsets[8] = {-1 * DijkstraLevel::kPitch + 0, 1 * DijkstraLevel::kPitch + 0,
                                  0 * DijkstraLevel::kPitch - 1,  0 * DijkstraLevel::kPitch + 1,

                                  -1 * DijkstraLevel::kPitch - 1, -1 * DijkstraLevel::kPitch + 1,
                                  1 * DijkstraLevel::kPitch - 1,  1 * DijkstraLevel::kPitch + 1};

int const kLevelCellCosts[8] = {256, 256, 256, 256,

                                362, 362, 362, 362};