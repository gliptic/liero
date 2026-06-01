#include "tcArchive.hpp"

#include <miniz.h>
#include <algorithm>
#include <cstring>

#include "../filesystem.hpp"

namespace TcArchive {

namespace {

// Recursively collect all files under a directory, with relative paths.
void collectFiles(FsNode node, const std::string& prefix,
                  std::vector<std::pair<std::string, std::vector<uint8_t>>>& out) {
  DirectoryListing listing = node.iter();
  for (auto& entry : listing) {
    FsNode child = node / entry.name;
    std::string relPath = prefix.empty() ? entry.name : prefix + "/" + entry.name;

    if (entry.isDir) {
      collectFiles(child, relPath, out);
    } else {
      try {
        auto r_ptr = child.toReader();
        io::Reader& r = *r_ptr;
        std::vector<uint8_t> data;
        uint8_t buf[4096];
        for (;;) {
          std::size_t got = r.try_get(buf, sizeof(buf));
          if (got == 0) break;
          data.insert(data.end(), buf, buf + got);
        }
        out.emplace_back(relPath, std::move(data));
      } catch (...) {
        // Skip files that can't be read
      }
    }
  }
}

}  // namespace

uint32_t computeHash(FsNode root) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  collectFiles(root, "", files);

  // Sort by filename for deterministic ordering
  std::sort(files.begin(), files.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  // FNV-1a hash over all file names and contents
  uint32_t hash = 2166136261u;
  for (auto& [name, data] : files) {
    for (char c : name) {
      hash ^= static_cast<uint8_t>(c);
      hash *= 16777619u;
    }
    for (uint8_t b : data) {
      hash ^= b;
      hash *= 16777619u;
    }
  }
  return hash;
}

std::vector<uint8_t> pack(FsNode root) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  collectFiles(root, "", files);

  // Sort for deterministic ordering
  std::sort(files.begin(), files.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  // Build raw archive: [numFiles(4)] + per file [nameLen(2) | name | dataLen(4) | data]
  std::vector<uint8_t> raw;
  uint32_t numFiles = static_cast<uint32_t>(files.size());
  raw.resize(4);
  std::memcpy(raw.data(), &numFiles, 4);

  for (auto& [name, data] : files) {
    if (name.size() > UINT16_MAX) continue;  // Skip files with names too long to encode
    uint16_t nameLen = static_cast<uint16_t>(name.size());
    uint32_t dataLen = static_cast<uint32_t>(data.size());

    size_t offset = raw.size();
    raw.resize(offset + 2 + nameLen + 4 + dataLen);

    std::memcpy(raw.data() + offset, &nameLen, 2);
    std::memcpy(raw.data() + offset + 2, name.data(), nameLen);
    std::memcpy(raw.data() + offset + 2 + nameLen, &dataLen, 4);
    std::memcpy(raw.data() + offset + 2 + nameLen + 4, data.data(), dataLen);
  }

  // Compress
  mz_ulong compBound = mz_compressBound(static_cast<mz_ulong>(raw.size()));
  std::vector<uint8_t> compressed(compBound + 5);  // 1 byte flag + 4 byte uncompressed size
  compressed[0] = 1;                               // compressed flag
  uint32_t rawSize = static_cast<uint32_t>(raw.size());
  std::memcpy(compressed.data() + 1, &rawSize, 4);

  mz_ulong compSize = compBound;
  int status =
      mz_compress(compressed.data() + 5, &compSize, raw.data(), static_cast<mz_ulong>(raw.size()));
  if (status == MZ_OK) {
    compressed.resize(5 + compSize);
  } else {
    // Fallback: send uncompressed
    compressed.resize(5 + raw.size());
    compressed[0] = 0;
    std::memcpy(compressed.data() + 5, raw.data(), raw.size());
  }

  return compressed;
}

std::vector<FileEntry> unpack(const uint8_t* data, size_t len) {
  std::vector<FileEntry> result;
  if (len < 5) return result;

  bool isCompressed = data[0] != 0;
  uint32_t rawSize;
  std::memcpy(&rawSize, data + 1, 4);

  // Prevent decompression bombs: limit uncompressed size to 64 MB
  static constexpr uint32_t MaxUncompressedSize = 64 * 1024 * 1024;
  if (rawSize > MaxUncompressedSize) return result;

  std::vector<uint8_t> raw;
  if (isCompressed) {
    raw.resize(rawSize);
    mz_ulong destLen = rawSize;
    int status = mz_uncompress(raw.data(), &destLen, data + 5, static_cast<mz_ulong>(len - 5));
    if (status != MZ_OK) return result;
  } else {
    if (len - 5 < rawSize) return result;
    raw.assign(data + 5, data + 5 + rawSize);
  }

  // Parse archive
  if (raw.size() < 4) return result;
  uint32_t numFiles;
  std::memcpy(&numFiles, raw.data(), 4);

  size_t offset = 4;
  for (uint32_t i = 0; i < numFiles; ++i) {
    if (offset + 2 > raw.size()) break;
    uint16_t nameLen;
    std::memcpy(&nameLen, raw.data() + offset, 2);
    offset += 2;

    if (offset + nameLen + 4 > raw.size()) break;
    std::string name(reinterpret_cast<const char*>(raw.data() + offset), nameLen);
    offset += nameLen;

    uint32_t dataLen;
    std::memcpy(&dataLen, raw.data() + offset, 4);
    offset += 4;

    if (offset + dataLen > raw.size()) break;
    std::vector<uint8_t> fileData(raw.data() + offset, raw.data() + offset + dataLen);
    offset += dataLen;

    result.push_back({std::move(name), std::move(fileData)});
  }

  return result;
}

}  // namespace TcArchive
