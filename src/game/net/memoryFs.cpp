#include "memoryFs.hpp"

#include <algorithm>
#include <cstring>
#include <set>

#include "../io/stream.hpp"

namespace {

struct FsNodeMemDir : FsNodeImp {
  std::string path;
  MemoryFs* fs;

  FsNodeMemDir(std::string path, MemoryFs* fs) : path(std::move(path)), fs(fs) {}

  std::string const& FullPath() override { return path; }

  DirectoryListing Iter() override {
    // Collect immediate children of this directory
    std::string const kPrefix = path.empty() ? "" : path + "/";
    std::vector<NodeName> entries;
    std::set<std::string> seen;

    for (auto& [key, _] : fs->files) {
      if (key.size() <= kPrefix.size()) {
        continue;
      }
      if (!kPrefix.empty() && !key.starts_with(kPrefix)) {
        continue;
      }

      // Get the next path component after prefix
      auto rest = key.substr(kPrefix.size());
      auto slash = rest.find('/');
      std::string const kName = (slash == std::string::npos) ? rest : rest.substr(0, slash);
      bool const kIsDir = (slash != std::string::npos);

      if (seen.insert(kName).second) {
        entries.emplace_back(kName, kIsDir);
      }
    }

    return {std::move(entries)};
  }

  std::shared_ptr<FsNodeImp> Go(std::string const& name) override {
    std::string const kChildPath = path.empty() ? name : path + "/" + name;

    // Check if it's a file
    auto it = fs->files.find(kChildPath);
    if (it != fs->files.end()) {
      // It's a file — return a file node
      return std::make_shared<FsNodeMemDir>(kChildPath, fs);
    }

    // Check if it's a directory (any file starts with childPath + "/")
    std::string const kDirPrefix = kChildPath + "/";
    for (auto& [key, _] : fs->files) {
      if (key.starts_with(kDirPrefix)) {
        return std::make_shared<FsNodeMemDir>(kChildPath, fs);
      }
    }

    // Doesn't exist, but return a node anyway (exists() will return false)
    return std::make_shared<FsNodeMemDir>(kChildPath, fs);
  }

  std::unique_ptr<io::Reader> TryToReader() override {
    auto it = fs->files.find(path);
    if (it == fs->files.end()) {
      return nullptr;
    }

    // The MemoryFs owns the underlying byte buffer (in fs_->files) so we
    // can hand out a MemReader that just points into it.
    return std::make_unique<io::MemReader>(it->second);
  }

  std::unique_ptr<io::Writer> TryToWriter() override { return nullptr; }

  bool Exists() const override {
    // Exists as a file?
    if (fs->files.contains(path)) {
      return true;
    }
    // Exists as a directory?
    std::string const kPrefix = path + "/";
    for (auto& [key, _] : fs->files) {
      if (key.starts_with(kPrefix)) {
        return true;
      }
    }
    return path.empty();  // Root always exists
  }
};

}  // namespace

FsNode MemoryFs::Root() { return {std::make_shared<FsNodeMemDir>("", this)}; }
