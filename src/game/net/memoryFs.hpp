#pragma once

#include <map>
#include <string>
#include <vector>

#include "../filesystem.hpp"

// An in-memory filesystem backed by a map of path → data.
// Used to avoid writing received TC data to disk.
struct MemoryFs {
  std::map<std::string, std::vector<uint8_t>> files;

  // Create an FsNode representing the root of this memory filesystem.
  FsNode Root();
};
