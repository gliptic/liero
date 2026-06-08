#include "tcArchive.hpp"

#include <miniz.h>
#include <algorithm>
#include <cstring>
#include <utility>

#include "../filesystem.hpp"

namespace tc_archive {

namespace {

// Recursively collect all files under a directory, with relative paths.
// NOLINTNEXTLINE(misc-no-recursion) — TC archives mirror a real filesystem; recursion mirrors that structure.
void CollectFiles(const FsNode& node, const std::string& prefix,
                  std::vector<std::pair<std::string, std::vector<uint8_t>>>& out) {
  DirectoryListing listing = node.Iter();
  for (auto& entry : listing) {
    FsNode const kChild = node / entry.name;
    std::string const kRelPath = prefix.empty() ? entry.name : prefix + "/" + entry.name;

    if (entry.is_dir) {
      CollectFiles(kChild, kRelPath, out);
    } else {
      try {
        auto r_ptr = kChild.ToReader();
        io::Reader& r = *r_ptr;
        std::vector<uint8_t> data;
        uint8_t buf[4096];
        for (;;) {
          std::size_t const kGot = r.TryGet(buf, sizeof(buf));
          if (kGot == 0) break;
          data.insert(data.end(), buf, buf + kGot);
        }
        out.emplace_back(kRelPath, std::move(data));
      } catch (...) {  // NOLINT(bugprone-empty-catch) — best-effort archive build; unreadable files
                       // are simply omitted.
        // Skip files that can't be read
      }
    }
  }
}

}  // namespace

uint32_t ComputeHash(const FsNode& root) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  CollectFiles(root, "", files);

  // Sort by filename for deterministic ordering
  std::ranges::sort(files, [](const auto& a, const auto& b) { return a.first < b.first; });

  // FNV-1a hash over all file names and contents
  uint32_t hash = 2166136261U;
  for (auto& [name, data] : files) {
    for (char const kC : name) {
      hash ^= static_cast<uint8_t>(kC);
      hash *= 16777619U;
    }
    for (uint8_t const kB : data) {
      hash ^= kB;
      hash *= 16777619U;
    }
  }
  return hash;
}

std::vector<uint8_t> Pack(const FsNode& root) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  CollectFiles(root, "", files);

  // Sort for deterministic ordering
  std::ranges::sort(files, [](const auto& a, const auto& b) { return a.first < b.first; });

  // Build raw archive: [numFiles(4)] + per file [nameLen(2) | name | dataLen(4) | data]
  std::vector<uint8_t> raw;
  auto num_files = static_cast<uint32_t>(files.size());
  raw.resize(4);
  std::memcpy(raw.data(), &num_files, 4);

  for (auto& [name, data] : files) {
    if (name.size() > UINT16_MAX) continue;  // Skip files with names too long to encode
    auto name_len = static_cast<uint16_t>(name.size());
    auto data_len = static_cast<uint32_t>(data.size());

    size_t const kOffset = raw.size();
    raw.resize(kOffset + 2 + name_len + 4 + data_len);

    std::memcpy(raw.data() + kOffset, &name_len, 2);
    std::memcpy(raw.data() + kOffset + 2, name.data(), name_len);
    std::memcpy(raw.data() + kOffset + 2 + name_len, &data_len, 4);
    std::memcpy(raw.data() + kOffset + 2 + name_len + 4, data.data(), data_len);
  }

  // Compress
  mz_ulong const kCompBound = mz_compressBound(static_cast<mz_ulong>(raw.size()));
  std::vector<uint8_t> compressed(kCompBound + 5);  // 1 byte flag + 4 byte uncompressed size
  compressed[0] = 1;                                // compressed flag
  auto raw_size = static_cast<uint32_t>(raw.size());
  std::memcpy(compressed.data() + 1, &raw_size, 4);

  mz_ulong comp_size = kCompBound;
  int const kStatus =
      mz_compress(compressed.data() + 5, &comp_size, raw.data(), static_cast<mz_ulong>(raw.size()));
  if (kStatus == MZ_OK) {
    compressed.resize(5 + comp_size);
  } else {
    // Fallback: send uncompressed
    compressed.resize(5 + raw.size());
    compressed[0] = 0;
    std::memcpy(compressed.data() + 5, raw.data(), raw.size());
  }

  return compressed;
}

std::vector<FileEntry> Unpack(const uint8_t* data, size_t len) {
  std::vector<FileEntry> result;
  if (len < 5) return result;

  bool const kIsCompressed = data[0] != 0;
  uint32_t raw_size = 0;
  std::memcpy(&raw_size, data + 1, 4);

  // Prevent decompression bombs: limit uncompressed size to 64 MB
  static constexpr uint32_t kMaxUncompressedSize = 64 * 1024 * 1024;
  if (raw_size > kMaxUncompressedSize) return result;

  std::vector<uint8_t> raw;
  if (kIsCompressed) {
    raw.resize(raw_size);
    mz_ulong dest_len = raw_size;
    int const kStatus =
        mz_uncompress(raw.data(), &dest_len, data + 5, static_cast<mz_ulong>(len - 5));
    if (kStatus != MZ_OK) return result;
  } else {
    if (len - 5 < raw_size) return result;
    raw.assign(data + 5, data + 5 + raw_size);
  }

  // Parse archive
  if (raw.size() < 4) return result;
  uint32_t num_files = 0;
  std::memcpy(&num_files, raw.data(), 4);

  size_t offset = 4;
  for (uint32_t i = 0; i < num_files; ++i) {
    if (offset + 2 > raw.size()) break;
    uint16_t name_len = 0;
    std::memcpy(&name_len, raw.data() + offset, 2);
    offset += 2;

    if (offset + name_len + 4 > raw.size()) break;
    std::string name(reinterpret_cast<const char*>(raw.data() + offset), name_len);
    offset += name_len;

    uint32_t data_len = 0;
    std::memcpy(&data_len, raw.data() + offset, 4);
    offset += 4;

    if (offset + data_len > raw.size()) break;
    std::vector<uint8_t> file_data(raw.data() + offset, raw.data() + offset + data_len);
    offset += data_len;

    result.push_back({.name = std::move(name), .data = std::move(file_data)});
  }

  return result;
}

}  // namespace tc_archive
