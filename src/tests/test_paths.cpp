#include <catch2/catch_test_macros.hpp>

#include "game/filesystem.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

struct ScopedEnv {
    std::string key;
    std::string prev;
    bool hadPrev;
    ScopedEnv(char const* k, char const* v) : key(k) {
        if (const char* p = std::getenv(k)) { prev = p; hadPrev = true; }
        else { hadPrev = false; }
        setenv(k, v, 1);
    }
    ~ScopedEnv() {
        if (hadPrev) setenv(key.c_str(), prev.c_str(), 1);
        else unsetenv(key.c_str());
    }
};

struct TempDir {
    fs::path path;
    explicit TempDir(std::string const& suffix) {
        path = fs::temp_directory_path() / ("openliero_test_" + suffix);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
    std::string str() const { return path.string(); }
};

void touch(fs::path const& p) {
    fs::create_directories(p.parent_path());
    std::ofstream{p};
}

// Build a fake argv for passing to resolve().
struct Argv {
    std::vector<std::string> storage;
    std::vector<char*> ptrs;
    Argv(std::initializer_list<char const*> args) {
        storage.emplace_back("openliero"); // argv[0]
        for (auto a : args) storage.emplace_back(a);
        for (auto& s : storage) ptrs.push_back(s.data());
        ptrs.push_back(nullptr);
    }
    int argc() const { return (int)storage.size(); }
    char** argv() { return ptrs.data(); }
};

} // namespace

// ---------------------------------------------------------------------------
// systemDataRoot tests (env-var override, from Task 1)
// ---------------------------------------------------------------------------

TEST_CASE("paths::systemDataRoot honours OPENLIERO_DATADIR env var when set and existing") {
    ScopedEnv env("OPENLIERO_DATADIR", OPENLIERO_TEST_DATA_DIR);

    FsNode node = paths::systemDataRoot();
    REQUIRE(static_cast<bool>(node));
    REQUIRE(node.exists());
    REQUIRE((node / "TC").exists());
}

TEST_CASE("paths::systemDataRoot fullPath matches OPENLIERO_DATADIR env var") {
    ScopedEnv env("OPENLIERO_DATADIR", OPENLIERO_TEST_DATA_DIR);

    FsNode node = paths::systemDataRoot();
    REQUIRE(static_cast<bool>(node));
    REQUIRE(node.fullPath() == std::string(OPENLIERO_TEST_DATA_DIR));
}

TEST_CASE("paths::systemDataRoot falls back when OPENLIERO_DATADIR points at nonexistent path") {
    ScopedEnv env("OPENLIERO_DATADIR", "/nonexistent/openliero/datadir/for/test");

    FsNode node = paths::systemDataRoot();
    if (node) {
        REQUIRE(node.fullPath() != std::string("/nonexistent/openliero/datadir/for/test"));
    }
}

// ---------------------------------------------------------------------------
// paths::resolve() tests
// ---------------------------------------------------------------------------

TEST_CASE("paths::resolve --config-root sets both nodes to the given path") {
    TempDir root("configroot");

    Argv a{"--config-root", root.str().c_str()};
    auto r = paths::resolve(a.argc(), a.argv(), root.str());

    REQUIRE(r.configNode.fullPath() == root.str());
    REQUIRE(r.userConfigNode.fullPath() == root.str());
    REQUIRE(r.port == 0);
    REQUIRE(r.positionalArgs.empty());
}

TEST_CASE("paths::resolve portable.txt sets both nodes to basePath") {
    TempDir base("portable_base");
    touch(base.path / "portable.txt");

    Argv a{};
    auto r = paths::resolve(a.argc(), a.argv(), base.str());

    REQUIRE(r.configNode.fullPath() == base.str());
    REQUIRE(r.userConfigNode.fullPath() == base.str());
}

TEST_CASE("paths::resolve XDG: user shadows system file of same name") {
    TempDir userDir("xdg_user");
    TempDir sysDir("xdg_system");

    // Both have Setups/liero.cfg but with different content.
    touch(sysDir.path / "Setups" / "liero.cfg");
    std::ofstream{sysDir.path / "Setups" / "liero.cfg"} << "system";

    touch(userDir.path / "Setups" / "liero.cfg");
    std::ofstream{userDir.path / "Setups" / "liero.cfg"} << "user";

    ScopedEnv envU("OPENLIERO_TEST_USER_DIR", userDir.str().c_str());
    ScopedEnv envS("OPENLIERO_DATADIR",  sysDir.str().c_str());

    // No portable.txt, no --config-root => XDG split.
    TempDir base("xdg_base_nosplit"); // basePath with no portable.txt
    Argv a{};
    auto r = paths::resolve(a.argc(), a.argv(), base.str());

    REQUIRE(r.userConfigNode.fullPath() == userDir.str());

    // Read through configNode should prefer user copy.
    auto reader = (r.configNode / "Setups" / "liero.cfg").toReader();
    REQUIRE(reader != nullptr);
    std::string content(4, '\0');
    reader->get(reinterpret_cast<uint8_t*>(content.data()), 4);
    REQUIRE(content == "user");
}

TEST_CASE("paths::resolve XDG: writer routes to user dir") {
    TempDir userDir("xdg_write_user");
    TempDir sysDir("xdg_write_system");

    ScopedEnv envU("OPENLIERO_TEST_USER_DIR", userDir.str().c_str());
    ScopedEnv envS("OPENLIERO_DATADIR",  sysDir.str().c_str());

    TempDir base("xdg_write_base");
    Argv a{};
    auto r = paths::resolve(a.argc(), a.argv(), base.str());

    // Write a file through userConfigNode.
    auto writer = (r.userConfigNode / "Setups" / "liero.cfg").toWriter();
    REQUIRE(writer != nullptr);

    // File should exist under userDir, not sysDir.
    REQUIRE(fs::exists(userDir.path / "Setups" / "liero.cfg"));
    REQUIRE(!fs::exists(sysDir.path / "Setups" / "liero.cfg"));
}

TEST_CASE("paths::resolve: --port is parsed") {
    TempDir base("port_base");
    Argv a{"--port", "12345"};
    auto r = paths::resolve(a.argc(), a.argv(), base.str());
    REQUIRE(r.port == 12345);
}

TEST_CASE("paths::resolve: positional args are collected") {
    TempDir base("pos_base");
    Argv a{"myTC"};
    auto r = paths::resolve(a.argc(), a.argv(), base.str());
    REQUIRE(r.positionalArgs.size() == 1);
    REQUIRE(r.positionalArgs[0] == "myTC");
}

TEST_CASE("paths::resolve: --flag=value form is accepted") {
    TempDir root("eqform");
    Argv a{("--config-root=" + root.str()).c_str(), "--port=9000"};
    auto r = paths::resolve(a.argc(), a.argv(), root.str());
    REQUIRE(r.configNode.fullPath() == root.str());
    REQUIRE(r.userConfigNode.fullPath() == root.str());
    REQUIRE(r.port == 9000);
}

TEST_CASE("paths::resolve: --config-root refuses to swallow a following flag") {
    // Typo case: user writes `--config-root --port 1234`. Without the guard,
    // configRoot would be set to "--port" and the game would create a
    // directory literally named "--port" under cwd.
    TempDir base("noswallow");
    Argv a{"--config-root", "--port", "1234"};
    auto r = paths::resolve(a.argc(), a.argv(), base.str());
    // --config-root rejected (no usable value), so we fall through to the
    // XDG branch — userConfigNode is the user dir, not "--port".
    REQUIRE(r.userConfigNode.fullPath() != std::string("--port"));
    REQUIRE(r.port == 1234);
}

// ---------------------------------------------------------------------------
// shadowsSystem tests
// ---------------------------------------------------------------------------

TEST_CASE("paths::shadowsSystem flags shipped files in XDG mode") {
    TempDir userDir("shadow_user");
    TempDir sysDir("shadow_sys");
    touch(sysDir.path / "Profiles" / "Default.toml");
    ScopedEnv envS("OPENLIERO_DATADIR", sysDir.str().c_str());

    FsNode userRoot(userDir.str());
    REQUIRE(paths::shadowsSystem(userRoot, "Profiles", "Default.toml"));
    REQUIRE(!paths::shadowsSystem(userRoot, "Profiles", "MyOwnName.toml"));
}

TEST_CASE("paths::shadowsSystem flags auto-managed names regardless of system layer") {
    TempDir userDir("shadow_auto_user");
    TempDir sysDir("shadow_auto_sys");
    ScopedEnv envS("OPENLIERO_DATADIR", sysDir.str().c_str());

    // liero.cfg is the auto-write target; even with no shipped copy it
    // must remain reserved so user Save As can't clobber it.
    FsNode userRoot(userDir.str());
    REQUIRE(paths::shadowsSystem(userRoot, "Setups", "liero.cfg"));
    REQUIRE(paths::shadowsSystem(userRoot, "Setups", "LIERO.CFG"));
    REQUIRE(!paths::shadowsSystem(userRoot, "Setups", "myoptions.cfg"));
}

TEST_CASE("paths::shadowsSystem skips on-disk check in portable mode") {
    // Portable / --config-root pointing at the install dir: userRoot
    // and the system root are the same directory, so the user's own
    // previously-saved file mustn't be treated as a shipped collision.
    TempDir sharedDir("shadow_portable");
    touch(sharedDir.path / "Setups" / "myoptions.cfg");
    ScopedEnv envS("OPENLIERO_DATADIR", sharedDir.str().c_str());

    FsNode userRoot(sharedDir.str());
    REQUIRE(!paths::shadowsSystem(userRoot, "Setups", "myoptions.cfg"));
    // Auto-managed names are still reserved.
    REQUIRE(paths::shadowsSystem(userRoot, "Setups", "liero.cfg"));
}
