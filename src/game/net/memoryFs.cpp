#include "memoryFs.hpp"

#include <algorithm>
#include <cstring>
#include <set>

#include "../io/stream.hpp"

namespace {

struct FsNodeMemDir : FsNodeImp {
  std::string path_;
  MemoryFs* fs_;

  FsNodeMemDir(std::string path, MemoryFs* fs) : path_(std::move(path)), fs_(fs) {}

  std::string const& fullPath() override { return path_; }

  DirectoryListing iter() override {
    // Collect immediate children of this directory
    std::string prefix = path_.empty() ? "" : path_ + "/";
    std::vector<NodeName> entries;
    std::set<std::string> seen;

    for (auto& [key, _] : fs_->files) {
      if (key.size() <= prefix.size()) continue;
      if (!prefix.empty() && key.compare(0, prefix.size(), prefix) != 0) continue;

      // Get the next path component after prefix
      auto rest = key.substr(prefix.size());
      auto slash = rest.find('/');
      std::string name = (slash == std::string::npos) ? rest : rest.substr(0, slash);
      bool isDir = (slash != std::string::npos);

      if (seen.insert(name).second) {
        entries.push_back(NodeName(name, isDir));
      }
    }

    return DirectoryListing(std::move(entries));
  }

  std::shared_ptr<FsNodeImp> go(std::string const& name) override {
    std::string childPath = path_.empty() ? name : path_ + "/" + name;

    // Check if it's a file
    auto it = fs_->files.find(childPath);
    if (it != fs_->files.end()) {
      // It's a file — return a file node
      return std::make_shared<FsNodeMemDir>(childPath, fs_);
    }

    // Check if it's a directory (any file starts with childPath + "/")
    std::string dirPrefix = childPath + "/";
    for (auto& [key, _] : fs_->files) {
      if (key.compare(0, dirPrefix.size(), dirPrefix) == 0) {
        return std::make_shared<FsNodeMemDir>(childPath, fs_);
      }
    }

    // Doesn't exist, but return a node anyway (exists() will return false)
    return std::make_shared<FsNodeMemDir>(childPath, fs_);
  }

  std::unique_ptr<io::Reader> tryToReader() override {
    auto it = fs_->files.find(path_);
    if (it == fs_->files.end()) return nullptr;

    // The MemoryFs owns the underlying byte buffer (in fs_->files) so we
    // can hand out a MemReader that just points into it.
    return std::make_unique<io::MemReader>(it->second);
  }

  std::unique_ptr<io::Writer> tryToWriter() override { return nullptr; }

  bool exists() const override {
    // Exists as a file?
    if (fs_->files.count(path_)) return true;
    // Exists as a directory?
    std::string prefix = path_ + "/";
    for (auto& [key, _] : fs_->files) {
      if (key.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return path_.empty();  // Root always exists
  }
};

}  // namespace

FsNode MemoryFs::root() { return FsNode(std::make_shared<FsNodeMemDir>("", this)); }
