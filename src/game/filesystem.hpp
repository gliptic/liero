#pragma once

#include <miniz.h>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "io/stream.hpp"

std::string ChangeLeaf(std::string const& path, std::string const& new_leaf);
std::string GetRoot(std::string const& path);
std::string GetLeaf(std::string const& path);
std::string GetBasename(std::string const& path);
std::string GetExtension(std::string const& path);
std::string ToUpperCase(std::string str);
std::string JoinPath(std::string const& root, std::string const& leaf);

FILE* TolerantFOpen(std::string const& name, char const* mode);

std::size_t FileLength(FILE* f);
bool CreateDirectories(std::string const& dir);

struct NodeName {
  NodeName(std::string name_init, bool is_dir_init)
      : name(std::move(name_init)), is_dir(is_dir_init) {}

  std::string name;
  bool is_dir;
};

struct DirectoryListing {
  std::vector<NodeName> subs;

  DirectoryListing(DirectoryListing&& other) : subs(std::move(other.subs)) {}

  DirectoryListing& operator=(DirectoryListing&& other) {
    subs = std::move(other.subs);
    return *this;
  }

  DirectoryListing(std::string const& dir);
  DirectoryListing(std::vector<NodeName>&& subs_init);
  ~DirectoryListing();

  DirectoryListing operator|(DirectoryListing const& other) {
    DirectoryListing ret(std::move(subs));

    ret.subs.insert(ret.subs.end(), other.subs.begin(), other.subs.end());
    ret.Sort();
    return ret;
  }

  // Range-for support: range-for looks up `begin`/`end` via ADL or as
  // members, so these must stay lower_case. NOLINT keeps the
  // identifier-naming check from renaming them on future tidy passes.
  // NOLINTNEXTLINE(readability-identifier-naming)
  std::vector<NodeName>::iterator begin() { return subs.begin(); }

  // NOLINTNEXTLINE(readability-identifier-naming)
  std::vector<NodeName>::iterator end() { return subs.end(); }

  void Sort();
};

struct FsNodeImp {
  virtual ~FsNodeImp() = default;
  virtual std::string const& FullPath() = 0;
  virtual DirectoryListing Iter() = 0;
  virtual std::shared_ptr<FsNodeImp> Go(std::string const& name) = 0;
  virtual std::unique_ptr<io::Reader> TryToReader() = 0;
  virtual std::unique_ptr<io::Writer> TryToWriter() = 0;
  virtual bool Exists() const = 0;
};

struct FsNode {
  std::shared_ptr<FsNodeImp> imp;

  FsNode() {}

  explicit FsNode(std::string const& path);

  FsNode(FsNode const& other) = default;
  FsNode& operator=(FsNode const& other) = default;

  FsNode(FsNode&& other) : imp(std::move(other.imp)) {}

  FsNode(std::shared_ptr<FsNodeImp> imp) : imp(std::move(imp)) {}

  FsNode& operator=(FsNode&& other) {
    imp = std::move(other.imp);
    return *this;
  }

  operator void*() const { return imp.get(); }

  std::string const& FullPath() const { return imp->FullPath(); }

  DirectoryListing Iter() const { return imp->Iter(); }

  FsNode operator/(std::string const& name) const { return FsNode(imp->Go(name)); }

  bool Exists() const { return imp && imp->Exists(); }

  std::unique_ptr<io::Reader> ToReader() const {
    auto r = imp->TryToReader();
    if (!r) throw std::runtime_error("Could not read " + FullPath());
    return r;
  }

  std::unique_ptr<io::Writer> ToWriter() const {
    auto w = imp->TryToWriter();
    if (!w) throw std::runtime_error("Could not write " + FullPath());
    return w;
  }
};

namespace paths {
// Writable user data root. Always returns a non-empty node whose
// directory has been created on disk. Backed by SDL_GetPrefPath.
FsNode UserDataRoot();

// Read-only stock data root. Resolution order:
//   1. OPENLIERO_DATADIR compile-time macro, if defined and existing.
//   2. SDL_GetBasePath() (binary-adjacent), if it has stock content.
// Returns an empty FsNode (!exists()) if neither resolves.
FsNode SystemDataRoot();

// True if Save As of `leaf` into `subdir` of the user dir would
// shadow either a shipped file or one of the auto-managed names
// the game writes itself (e.g. `Setups/liero.cfg`). Used by the
// Save As dialogs to refuse reserved names. `userRoot` is the
// user's writable root; when it equals systemDataRoot() (portable
// mode, `--config-root` aimed at the install dir) there is no
// separate read-only layer to shadow and the user can freely
// overwrite their own files, so the on-disk check is skipped.
bool ShadowsSystem(FsNode const& user_root, std::string const& subdir, std::string const& leaf);
}  // namespace paths

struct ResolvedPaths {
  FsNode config_node;                        // merged (user + system) for reads
  FsNode user_config_node;                   // user dir only, for writes
  uint16_t port;                             // from --port, 0 if not given
  std::vector<std::string> positional_args;  // non-flag argv entries
};

namespace paths {
// Parse argc/argv for --config-root, --port, and positional args,
// then build configNode and userConfigNode according to the algorithm:
//   --config-root <p>  -> both nodes point at p (portable/Emscripten mode)
//   portable.txt in basePath -> both nodes point at basePath
//   otherwise          -> configNode = join(user, system); userConfigNode = user
//
// basePath overrides SDL_GetBasePath() and is used by tests.
ResolvedPaths Resolve(int argc, char* argv[], std::string const& base_path = std::string());
}  // namespace paths
