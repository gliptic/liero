#include "tcArchive.hpp"

#include <miniz.h>
#include <algorithm>
#include <cstring>

#include "../filesystem.hpp"

namespace tc_archive {

namespace {

// Recursively collect all files under a directory, with relative paths.
void CollectFiles(FsNode node, const std::string& prefix,
                  std::vector<std::pair<std::string, std::vector<uint8_t>>>& out) {
  DirectoryListing listing = node.Iter();
  for (auto& entry : listing) {
    FsNode child = node / entry.name;
    std::string rel_path = prefix.empty() ? entry.name : prefix + "/" + entry.name;

    if (entry.is_dir) {
      CollectFiles(child, rel_path, out);
    } else {
      try {
        auto r_ptr = child.ToReader();
        io::Reader& r = *r_ptr;
        std::vector<uint8_t> data;
        uint8_t buf[4096];
        for (;;) {
          std::size_t got = r.TryGet(buf, sizeof(buf));
          if (got == 0) break;
          data.insert(data.end(), buf, buf + got);
        }
        out.emplace_back(rel_path, std::move(data));
      } catch (...) {
        // Skip files that can't be read
      }
    }
  }
}

}  // namespace

uint32_t ComputeHash(FsNode root) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  CollectFiles(root, "", files);

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

std::vector<uint8_t> Pack(FsNode root) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  CollectFiles(root, "", files);

  // Sort for deterministic ordering
  std::sort(files.begin(), files.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  // Build raw archive: [numFiles(4)] + per file [nameLen(2) | name | dataLen(4) | data]
  std::vector<uint8_t> raw;
  uint32_t num_files = static_cast<uint32_t>(files.size());
  raw.resize(4);
  std::memcpy(raw.data(), &num_files, 4);

  for (auto& [name, data] : files) {
    if (name.size() > UINT16_MAX) continue;  // Skip files with names too long to encode
    uint16_t name_len = static_cast<uint16_t>(name.size());
    uint32_t data_len = static_cast<uint32_t>(data.size());

    size_t offset = raw.size();
    raw.resize(offset + 2 + name_len + 4 + data_len);

    std::memcpy(raw.data() + offset, &name_len, 2);
    std::memcpy(raw.data() + offset + 2, name.data(), name_len);
    std::memcpy(raw.data() + offset + 2 + name_len, &data_len, 4);
    std::memcpy(raw.data() + offset + 2 + name_len + 4, data.data(), data_len);
  }

  // Compress
  mz_ulong comp_bound = mz_compressBound(static_cast<mz_ulong>(raw.size()));
  std::vector<uint8_t> compressed(comp_bound + 5);  // 1 byte flag + 4 byte uncompressed size
  compressed[0] = 1;                                // compressed flag
  uint32_t raw_size = static_cast<uint32_t>(raw.size());
  std::memcpy(compressed.data() + 1, &raw_size, 4);

  mz_ulong comp_size = comp_bound;
  int status =
      mz_compress(compressed.data() + 5, &comp_size, raw.data(), static_cast<mz_ulong>(raw.size()));
  if (status == MZ_OK) {
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

  bool is_compressed = data[0] != 0;
  uint32_t raw_size;
  std::memcpy(&raw_size, data + 1, 4);

  // Prevent decompression bombs: limit uncompressed size to 64 MB
  static constexpr uint32_t kMaxUncompressedSize = 64 * 1024 * 1024;
  if (raw_size > kMaxUncompressedSize) return result;

  std::vector<uint8_t> raw;
  if (is_compressed) {
    raw.resize(raw_size);
    mz_ulong dest_len = raw_size;
    int status = mz_uncompress(raw.data(), &dest_len, data + 5, static_cast<mz_ulong>(len - 5));
    if (status != MZ_OK) return result;
  } else {
    if (len - 5 < raw_size) return result;
    raw.assign(data + 5, data + 5 + raw_size);
  }

  // Parse archive
  if (raw.size() < 4) return result;
  uint32_t num_files;
  std::memcpy(&num_files, raw.data(), 4);

  size_t offset = 4;
  for (uint32_t i = 0; i < num_files; ++i) {
    if (offset + 2 > raw.size()) break;
    uint16_t name_len;
    std::memcpy(&name_len, raw.data() + offset, 2);
    offset += 2;

    if (offset + name_len + 4 > raw.size()) break;
    std::string name(reinterpret_cast<const char*>(raw.data() + offset), name_len);
    offset += name_len;

    uint32_t data_len;
    std::memcpy(&data_len, raw.data() + offset, 4);
    offset += 4;

    if (offset + data_len > raw.size()) break;
    std::vector<uint8_t> file_data(raw.data() + offset, raw.data() + offset + data_len);
    offset += data_len;

    result.push_back({std::move(name), std::move(file_data)});
  }

  return result;
}

}  // namespace tc_archive
