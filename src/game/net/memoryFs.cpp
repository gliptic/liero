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
    std::string prefix = path.empty() ? "" : path + "/";
    std::vector<NodeName> entries;
    std::set<std::string> seen;

    for (auto& [key, _] : fs->files) {
      if (key.size() <= prefix.size()) continue;
      if (!prefix.empty() && key.compare(0, prefix.size(), prefix) != 0) continue;

      // Get the next path component after prefix
      auto rest = key.substr(prefix.size());
      auto slash = rest.find('/');
      std::string name = (slash == std::string::npos) ? rest : rest.substr(0, slash);
      bool is_dir = (slash != std::string::npos);

      if (seen.insert(name).second) {
        entries.push_back(NodeName(name, is_dir));
      }
    }

    return DirectoryListing(std::move(entries));
  }

  std::shared_ptr<FsNodeImp> Go(std::string const& name) override {
    std::string child_path = path.empty() ? name : path + "/" + name;

    // Check if it's a file
    auto it = fs->files.find(child_path);
    if (it != fs->files.end()) {
      // It's a file — return a file node
      return std::make_shared<FsNodeMemDir>(child_path, fs);
    }

    // Check if it's a directory (any file starts with childPath + "/")
    std::string dir_prefix = child_path + "/";
    for (auto& [key, _] : fs->files) {
      if (key.compare(0, dir_prefix.size(), dir_prefix) == 0) {
        return std::make_shared<FsNodeMemDir>(child_path, fs);
      }
    }

    // Doesn't exist, but return a node anyway (exists() will return false)
    return std::make_shared<FsNodeMemDir>(child_path, fs);
  }

  std::unique_ptr<io::Reader> TryToReader() override {
    auto it = fs->files.find(path);
    if (it == fs->files.end()) return nullptr;

    // The MemoryFs owns the underlying byte buffer (in fs_->files) so we
    // can hand out a MemReader that just points into it.
    return std::make_unique<io::MemReader>(it->second);
  }

  std::unique_ptr<io::Writer> TryToWriter() override { return nullptr; }

  bool Exists() const override {
    // Exists as a file?
    if (fs->files.count(path)) return true;
    // Exists as a directory?
    std::string prefix = path + "/";
    for (auto& [key, _] : fs->files) {
      if (key.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return path.empty();  // Root always exists
  }
};

}  // namespace

FsNode MemoryFs::Root() { return FsNode(std::make_shared<FsNodeMemDir>("", this)); }
