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
    if (had_prev) {
      setenv(key.c_str(), prev.c_str(), 1);
    } else {
      unsetenv(key.c_str());
    }
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
    for (const auto* a : args) {
      storage.emplace_back(a);
    }
    for (auto& s : storage) {
      ptrs.push_back(s.data());
    }
    ptrs.push_back(nullptr);
  }
  int Argc() const { return static_cast<int>(storage.size()); }
  char** Data() { return ptrs.data(); }
};

}  // namespace

// ---------------------------------------------------------------------------
// systemDataRoot tests (env-var override, from Task 1)
// ---------------------------------------------------------------------------

TEST_CASE("paths::systemDataRoot honours OPENLIERO_DATADIR env var when set and existing") {
  ScopedEnv const kEnv("OPENLIERO_DATADIR", OPENLIERO_TEST_DATA_DIR);

  FsNode const kNode = paths::SystemDataRoot();
  REQUIRE(static_cast<bool>(kNode));
  REQUIRE(kNode.Exists());
  REQUIRE((kNode / "TC").Exists());
}

TEST_CASE("paths::systemDataRoot fullPath matches OPENLIERO_DATADIR env var") {
  ScopedEnv const kEnv("OPENLIERO_DATADIR", OPENLIERO_TEST_DATA_DIR);

  FsNode const kNode = paths::SystemDataRoot();
  REQUIRE(static_cast<bool>(kNode));
  REQUIRE(kNode.FullPath() == std::string(OPENLIERO_TEST_DATA_DIR));
}

TEST_CASE("paths::systemDataRoot falls back when OPENLIERO_DATADIR points at nonexistent path") {
  ScopedEnv const kEnv("OPENLIERO_DATADIR", "/nonexistent/openliero/datadir/for/test");

  FsNode const kNode = paths::SystemDataRoot();
  if (kNode) {
    REQUIRE(kNode.FullPath() != std::string("/nonexistent/openliero/datadir/for/test"));
  }
}

// ---------------------------------------------------------------------------
// paths::resolve() tests
// ---------------------------------------------------------------------------

TEST_CASE("paths::resolve --config-root sets both nodes to the given path") {
  TempDir const kRoot("configroot");

  Argv a{"--config-root", kRoot.Str().c_str()};
  auto r = paths::Resolve(a.Argc(), a.Data(), kRoot.Str());

  REQUIRE(r.config_node.FullPath() == kRoot.Str());
  REQUIRE(r.user_config_node.FullPath() == kRoot.Str());
  REQUIRE(r.port == 0);
  REQUIRE(r.positional_args.empty());
}

TEST_CASE("paths::resolve portable.txt sets both nodes to basePath") {
  TempDir const kBase("portable_base");
  Touch(kBase.path / "portable.txt");

  Argv a{};
  auto r = paths::Resolve(a.Argc(), a.Data(), kBase.Str());

  REQUIRE(r.config_node.FullPath() == kBase.Str());
  REQUIRE(r.user_config_node.FullPath() == kBase.Str());
}

TEST_CASE("paths::resolve XDG: user shadows system file of same name") {
  TempDir const kUserDir("xdg_user");
  TempDir const kSysDir("xdg_system");

  // Both have Setups/liero.cfg but with different content.
  Touch(kSysDir.path / "Setups" / "liero.cfg");
  std::ofstream{kSysDir.path / "Setups" / "liero.cfg"} << "system";

  Touch(kUserDir.path / "Setups" / "liero.cfg");
  std::ofstream{kUserDir.path / "Setups" / "liero.cfg"} << "user";

  ScopedEnv const kEnvU("OPENLIERO_TEST_USER_DIR", kUserDir.Str().c_str());
  ScopedEnv const kEnvS("OPENLIERO_DATADIR", kSysDir.Str().c_str());

  // No portable.txt, no --config-root => XDG split.
  TempDir const kBase("xdg_base_nosplit");  // basePath with no portable.txt
  Argv a{};
  auto r = paths::Resolve(a.Argc(), a.Data(), kBase.Str());

  REQUIRE(r.user_config_node.FullPath() == kUserDir.Str());

  // Read through configNode should prefer user copy.
  auto reader = (r.config_node / "Setups" / "liero.cfg").ToReader();
  REQUIRE(reader != nullptr);
  std::string content(4, '\0');
  reader->Get(reinterpret_cast<uint8_t*>(content.data()), 4);
  REQUIRE(content == "user");
}

TEST_CASE("paths::resolve XDG: writer routes to user dir") {
  TempDir const kUserDir("xdg_write_user");
  TempDir const kSysDir("xdg_write_system");

  ScopedEnv const kEnvU("OPENLIERO_TEST_USER_DIR", kUserDir.Str().c_str());
  ScopedEnv const kEnvS("OPENLIERO_DATADIR", kSysDir.Str().c_str());

  TempDir const kBase("xdg_write_base");
  Argv a{};
  auto r = paths::Resolve(a.Argc(), a.Data(), kBase.Str());

  // Write a file through userConfigNode.
  auto writer = (r.user_config_node / "Setups" / "liero.cfg").ToWriter();
  REQUIRE(writer != nullptr);

  // File should exist under userDir, not sysDir.
  REQUIRE(fs::exists(kUserDir.path / "Setups" / "liero.cfg"));
  REQUIRE(!fs::exists(kSysDir.path / "Setups" / "liero.cfg"));
}

TEST_CASE("paths::resolve: --port is parsed") {
  TempDir const kBase("port_base");
  Argv a{"--port", "12345"};
  auto r = paths::Resolve(a.Argc(), a.Data(), kBase.Str());
  REQUIRE(r.port == 12345);
}

TEST_CASE("paths::resolve: positional args are collected") {
  TempDir const kBase("pos_base");
  Argv a{"myTC"};
  auto r = paths::Resolve(a.Argc(), a.Data(), kBase.Str());
  REQUIRE(r.positional_args.size() == 1);
  REQUIRE(r.positional_args[0] == "myTC");
}

TEST_CASE("paths::resolve: --flag=value form is accepted") {
  TempDir const kRoot("eqform");
  Argv a{("--config-root=" + kRoot.Str()).c_str(), "--port=9000"};
  auto r = paths::Resolve(a.Argc(), a.Data(), kRoot.Str());
  REQUIRE(r.config_node.FullPath() == kRoot.Str());
  REQUIRE(r.user_config_node.FullPath() == kRoot.Str());
  REQUIRE(r.port == 9000);
}

TEST_CASE("paths::resolve: --config-root refuses to swallow a following flag") {
  // Typo case: user writes `--config-root --port 1234`. Without the guard,
  // configRoot would be set to "--port" and the game would create a
  // directory literally named "--port" under cwd.
  TempDir const kBase("noswallow");
  Argv a{"--config-root", "--port", "1234"};
  auto r = paths::Resolve(a.Argc(), a.Data(), kBase.Str());
  // --config-root rejected (no usable value), so we fall through to the
  // XDG branch — userConfigNode is the user dir, not "--port".
  REQUIRE(r.user_config_node.FullPath() != std::string("--port"));
  REQUIRE(r.port == 1234);
}

// ---------------------------------------------------------------------------
// shadowsSystem tests
// ---------------------------------------------------------------------------

TEST_CASE("paths::shadowsSystem flags shipped files in XDG mode") {
  TempDir const kUserDir("shadow_user");
  TempDir const kSysDir("shadow_sys");
  Touch(kSysDir.path / "Profiles" / "Default.toml");
  ScopedEnv const kEnvS("OPENLIERO_DATADIR", kSysDir.Str().c_str());

  FsNode const kUserRoot(kUserDir.Str());
  REQUIRE(paths::ShadowsSystem(kUserRoot, "Profiles", "Default.toml"));
  REQUIRE(!paths::ShadowsSystem(kUserRoot, "Profiles", "MyOwnName.toml"));
}

TEST_CASE("paths::shadowsSystem flags auto-managed names regardless of system layer") {
  TempDir const kUserDir("shadow_auto_user");
  TempDir const kSysDir("shadow_auto_sys");
  ScopedEnv const kEnvS("OPENLIERO_DATADIR", kSysDir.Str().c_str());

  // liero.cfg is the auto-write target; even with no shipped copy it
  // must remain reserved so user Save As can't clobber it.
  FsNode const kUserRoot(kUserDir.Str());
  REQUIRE(paths::ShadowsSystem(kUserRoot, "Setups", "liero.cfg"));
  REQUIRE(paths::ShadowsSystem(kUserRoot, "Setups", "LIERO.CFG"));
  REQUIRE(!paths::ShadowsSystem(kUserRoot, "Setups", "myoptions.cfg"));
}

TEST_CASE("paths::shadowsSystem skips on-disk check in portable mode") {
  // Portable / --config-root pointing at the install dir: userRoot
  // and the system root are the same directory, so the user's own
  // previously-saved file mustn't be treated as a shipped collision.
  TempDir const kSharedDir("shadow_portable");
  Touch(kSharedDir.path / "Setups" / "myoptions.cfg");
  ScopedEnv const kEnvS("OPENLIERO_DATADIR", kSharedDir.Str().c_str());

  FsNode const kUserRoot(kSharedDir.Str());
  REQUIRE(!paths::ShadowsSystem(kUserRoot, "Setups", "myoptions.cfg"));
  // Auto-managed names are still reserved.
  REQUIRE(paths::ShadowsSystem(kUserRoot, "Setups", "liero.cfg"));
}
