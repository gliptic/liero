#include <catch2/catch_test_macros.hpp>

#include "game/filesystem.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

struct ScopedEnv {
  std::string key;
  std::string prev;
  bool had_prev;
  ScopedEnv(char const* k, char const* v) : key(k) {
    if (const char* p = std::getenv(k)) {
      prev = p;
      had_prev = true;
    } else {
      had_prev = false;
    }
    setenv(k, v, 1);
  }
  ~ScopedEnv() {
    if (had_prev)
      setenv(key.c_str(), prev.c_str(), 1);
    else
      unsetenv(key.c_str());
  }
};

struct TempDir {
  fs::path path;
  explicit TempDir(std::string const& suffix) {
    path = fs::temp_directory_path() / ("openliero_test_" + suffix);
    fs::create_directories(path);
  }
  ~TempDir() { fs::remove_all(path); }
  std::string Str() const { return path.string(); }
};

void Touch(fs::path const& p) {
  fs::create_directories(p.parent_path());
  std::ofstream{p};
}

// Build a fake argv for passing to resolve().
struct Argv {
  std::vector<std::string> storage;
  std::vector<char*> ptrs;
  Argv(std::initializer_list<char const*> args) {
    storage.emplace_back("openliero");  // argv[0]
    for (auto a : args) storage.emplace_back(a);
    for (auto& s : storage) ptrs.push_back(s.data());
    ptrs.push_back(nullptr);
  }
  int Argc() const { return (int)storage.size(); }
  char** Data() { return ptrs.data(); }
};

}  // namespace

// ---------------------------------------------------------------------------
// systemDataRoot tests (env-var override, from Task 1)
// ---------------------------------------------------------------------------

TEST_CASE("paths::systemDataRoot honours OPENLIERO_DATADIR env var when set and existing") {
  ScopedEnv env("OPENLIERO_DATADIR", OPENLIERO_TEST_DATA_DIR);

  FsNode node = paths::SystemDataRoot();
  REQUIRE(static_cast<bool>(node));
  REQUIRE(node.Exists());
  REQUIRE((node / "TC").Exists());
}

TEST_CASE("paths::systemDataRoot fullPath matches OPENLIERO_DATADIR env var") {
  ScopedEnv env("OPENLIERO_DATADIR", OPENLIERO_TEST_DATA_DIR);

  FsNode node = paths::SystemDataRoot();
  REQUIRE(static_cast<bool>(node));
  REQUIRE(node.FullPath() == std::string(OPENLIERO_TEST_DATA_DIR));
}

TEST_CASE("paths::systemDataRoot falls back when OPENLIERO_DATADIR points at nonexistent path") {
  ScopedEnv env("OPENLIERO_DATADIR", "/nonexistent/openliero/datadir/for/test");

  FsNode node = paths::SystemDataRoot();
  if (node) {
    REQUIRE(node.FullPath() != std::string("/nonexistent/openliero/datadir/for/test"));
  }
}

// ---------------------------------------------------------------------------
// paths::resolve() tests
// ---------------------------------------------------------------------------

TEST_CASE("paths::resolve --config-root sets both nodes to the given path") {
  TempDir root("configroot");

  Argv a{"--config-root", root.Str().c_str()};
  auto r = paths::Resolve(a.Argc(), a.Data(), root.Str());

  REQUIRE(r.config_node.FullPath() == root.Str());
  REQUIRE(r.user_config_node.FullPath() == root.Str());
  REQUIRE(r.port == 0);
  REQUIRE(r.positional_args.empty());
}

TEST_CASE("paths::resolve portable.txt sets both nodes to basePath") {
  TempDir base("portable_base");
  Touch(base.path / "portable.txt");

  Argv a{};
  auto r = paths::Resolve(a.Argc(), a.Data(), base.Str());

  REQUIRE(r.config_node.FullPath() == base.Str());
  REQUIRE(r.user_config_node.FullPath() == base.Str());
}

TEST_CASE("paths::resolve XDG: user shadows system file of same name") {
  TempDir user_dir("xdg_user");
  TempDir sys_dir("xdg_system");

  // Both have Setups/liero.cfg but with different content.
  Touch(sys_dir.path / "Setups" / "liero.cfg");
  std::ofstream{sys_dir.path / "Setups" / "liero.cfg"} << "system";

  Touch(user_dir.path / "Setups" / "liero.cfg");
  std::ofstream{user_dir.path / "Setups" / "liero.cfg"} << "user";

  ScopedEnv env_u("OPENLIERO_TEST_USER_DIR", user_dir.Str().c_str());
  ScopedEnv env_s("OPENLIERO_DATADIR", sys_dir.Str().c_str());

  // No portable.txt, no --config-root => XDG split.
  TempDir base("xdg_base_nosplit");  // basePath with no portable.txt
  Argv a{};
  auto r = paths::Resolve(a.Argc(), a.Data(), base.Str());

  REQUIRE(r.user_config_node.FullPath() == user_dir.Str());

  // Read through configNode should prefer user copy.
  auto reader = (r.config_node / "Setups" / "liero.cfg").ToReader();
  REQUIRE(reader != nullptr);
  std::string content(4, '\0');
  reader->Get(reinterpret_cast<uint8_t*>(content.data()), 4);
  REQUIRE(content == "user");
}

TEST_CASE("paths::resolve XDG: writer routes to user dir") {
  TempDir user_dir("xdg_write_user");
  TempDir sys_dir("xdg_write_system");

  ScopedEnv env_u("OPENLIERO_TEST_USER_DIR", user_dir.Str().c_str());
  ScopedEnv env_s("OPENLIERO_DATADIR", sys_dir.Str().c_str());

  TempDir base("xdg_write_base");
  Argv a{};
  auto r = paths::Resolve(a.Argc(), a.Data(), base.Str());

  // Write a file through userConfigNode.
  auto writer = (r.user_config_node / "Setups" / "liero.cfg").ToWriter();
  REQUIRE(writer != nullptr);

  // File should exist under userDir, not sysDir.
  REQUIRE(fs::exists(user_dir.path / "Setups" / "liero.cfg"));
  REQUIRE(!fs::exists(sys_dir.path / "Setups" / "liero.cfg"));
}

TEST_CASE("paths::resolve: --port is parsed") {
  TempDir base("port_base");
  Argv a{"--port", "12345"};
  auto r = paths::Resolve(a.Argc(), a.Data(), base.Str());
  REQUIRE(r.port == 12345);
}

TEST_CASE("paths::resolve: positional args are collected") {
  TempDir base("pos_base");
  Argv a{"myTC"};
  auto r = paths::Resolve(a.Argc(), a.Data(), base.Str());
  REQUIRE(r.positional_args.size() == 1);
  REQUIRE(r.positional_args[0] == "myTC");
}

TEST_CASE("paths::resolve: --flag=value form is accepted") {
  TempDir root("eqform");
  Argv a{("--config-root=" + root.Str()).c_str(), "--port=9000"};
  auto r = paths::Resolve(a.Argc(), a.Data(), root.Str());
  REQUIRE(r.config_node.FullPath() == root.Str());
  REQUIRE(r.user_config_node.FullPath() == root.Str());
  REQUIRE(r.port == 9000);
}

TEST_CASE("paths::resolve: --config-root refuses to swallow a following flag") {
  // Typo case: user writes `--config-root --port 1234`. Without the guard,
  // configRoot would be set to "--port" and the game would create a
  // directory literally named "--port" under cwd.
  TempDir base("noswallow");
  Argv a{"--config-root", "--port", "1234"};
  auto r = paths::Resolve(a.Argc(), a.Data(), base.Str());
  // --config-root rejected (no usable value), so we fall through to the
  // XDG branch — userConfigNode is the user dir, not "--port".
  REQUIRE(r.user_config_node.FullPath() != std::string("--port"));
  REQUIRE(r.port == 1234);
}

// ---------------------------------------------------------------------------
// shadowsSystem tests
// ---------------------------------------------------------------------------

TEST_CASE("paths::shadowsSystem flags shipped files in XDG mode") {
  TempDir user_dir("shadow_user");
  TempDir sys_dir("shadow_sys");
  Touch(sys_dir.path / "Profiles" / "Default.toml");
  ScopedEnv env_s("OPENLIERO_DATADIR", sys_dir.Str().c_str());

  FsNode user_root(user_dir.Str());
  REQUIRE(paths::ShadowsSystem(user_root, "Profiles", "Default.toml"));
  REQUIRE(!paths::ShadowsSystem(user_root, "Profiles", "MyOwnName.toml"));
}

TEST_CASE("paths::shadowsSystem flags auto-managed names regardless of system layer") {
  TempDir user_dir("shadow_auto_user");
  TempDir sys_dir("shadow_auto_sys");
  ScopedEnv env_s("OPENLIERO_DATADIR", sys_dir.Str().c_str());

  // liero.cfg is the auto-write target; even with no shipped copy it
  // must remain reserved so user Save As can't clobber it.
  FsNode user_root(user_dir.Str());
  REQUIRE(paths::ShadowsSystem(user_root, "Setups", "liero.cfg"));
  REQUIRE(paths::ShadowsSystem(user_root, "Setups", "LIERO.CFG"));
  REQUIRE(!paths::ShadowsSystem(user_root, "Setups", "myoptions.cfg"));
}

TEST_CASE("paths::shadowsSystem skips on-disk check in portable mode") {
  // Portable / --config-root pointing at the install dir: userRoot
  // and the system root are the same directory, so the user's own
  // previously-saved file mustn't be treated as a shipped collision.
  TempDir shared_dir("shadow_portable");
  Touch(shared_dir.path / "Setups" / "myoptions.cfg");
  ScopedEnv env_s("OPENLIERO_DATADIR", shared_dir.Str().c_str());

  FsNode user_root(shared_dir.Str());
  REQUIRE(!paths::ShadowsSystem(user_root, "Setups", "myoptions.cfg"));
  // Auto-managed names are still reserved.
  REQUIRE(paths::ShadowsSystem(user_root, "Setups", "liero.cfg"));
}
