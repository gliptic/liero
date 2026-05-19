#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FsNode;

// Utilities for TC (total conversion) hashing and archive transfer.
namespace TcArchive {

// Compute a deterministic hash of a TC directory.
// Walks all files recursively in sorted filename order,
// hashing their contents with FNV-1a.
uint32_t computeHash(FsNode root);

// Pack a TC directory into a compressed archive blob.
// Archive format: [numFiles(4)] then for each file:
//   [nameLen(2) | name(nameLen) | dataLen(4) | data(dataLen)]
// The entire payload is compressed with miniz.
std::vector<uint8_t> pack(FsNode root);

// Unpack a compressed TC archive into a flat file map.
// Returns a list of (relative path, file data) pairs.
struct FileEntry {
  std::string name;
  std::vector<uint8_t> data;
};
std::vector<FileEntry> unpack(const uint8_t* data, size_t len);

}  // namespace TcArchive
