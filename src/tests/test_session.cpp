#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

#include "game.hpp"
#include "math.hpp"
#include "net/session.hpp"
#include "net/tcArchive.hpp"

struct SessionFixture {
  std::shared_ptr<Common> common;
  std::shared_ptr<Settings> settings;
  FsNode tc_root;

  SessionFixture() {
    PrecomputeTables();

    common = std::make_shared<Common>();
    tc_root = FsNode("data") / "TC" / "openliero";
    common->load(tc_root);

    settings = std::make_shared<Settings>();
    settings->lives = 10;
    settings->loading_time = 0;
    settings->random_level = true;
    settings->game_mode = Settings::kGmKillEmAll;
  }
};

// Poll both sessions until a condition is met or timeout
template <typename Pred>
static bool PollUntil(NetSession& a, NetSession& b, Pred pred, int max_ms = 3000) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(max_ms);
  while (!pred() && std::chrono::steady_clock::now() < deadline) {
    a.Update();
    b.Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return pred();
}

TEST_CASE("NetSession host and client connect and handshake", "[session]") {
  SessionFixture f;

  NetSession host(f.common, f.settings, f.tc_root);
  NetSession client(f.common, f.settings, f.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(host.State() == NetSession::kWaitingForPeer);

  REQUIRE(client.JoinGame("127.0.0.1", port));
  REQUIRE(client.State() == NetSession::kWaitingForPeer);

  // Poll until both reach Playing state
  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });

  REQUIRE(ready);
  REQUIRE(host.Rollback() != nullptr);
  REQUIRE(client.Rollback() != nullptr);

  // Both games should have the same RNG seed
  REQUIRE(host.Rollback()->game.rand == client.Rollback()->game.rand);
}

TEST_CASE("NetSession syncs host settings to client", "[session]") {
  SessionFixture f;

  auto settings_b = std::make_shared<Settings>(*f.settings);
  settings_b->lives = 99;
  settings_b->blood = 200;
  settings_b->loading_time = 50;
  settings_b->game_mode = Settings::kGmGameOfTag;
  settings_b->max_bonuses = 7;
  settings_b->time_to_lose = 999;
  settings_b->flags_to_win = 3;
  settings_b->load_change = true;
  // Modify some weapTable entries
  for (int i = 0; i < 40; ++i) settings_b->weap_table[i] = (i < 10) ? 2 : 0;

  NetSession host(f.common, f.settings, f.tc_root);
  NetSession client(f.common, settings_b, f.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  // Poll until both reach Playing — host settings are authoritative
  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });

  REQUIRE(ready);
  // Client should have received and applied host's settings
  REQUIRE(settings_b->lives == f.settings->lives);
  REQUIRE(settings_b->blood == f.settings->blood);
  REQUIRE(settings_b->loading_time == f.settings->loading_time);
  REQUIRE(settings_b->game_mode == f.settings->game_mode);
  REQUIRE(settings_b->max_bonuses == f.settings->max_bonuses);
  REQUIRE(settings_b->time_to_lose == f.settings->time_to_lose);
  REQUIRE(settings_b->flags_to_win == f.settings->flags_to_win);
  REQUIRE(settings_b->load_change == f.settings->load_change);
  for (int i = 0; i < 40; ++i) REQUIRE(settings_b->weap_table[i] == f.settings->weap_table[i]);
}

TEST_CASE("NetSession syncs worm colors and weapons between peers", "[session]") {
  SessionFixture f;

  // Give each player distinct colors and weapons in the network player slot
  auto settings_host = std::make_shared<Settings>(*f.settings);
  // Create distinct WormSettings objects so shared_ptr sharing doesn't cause issues
  for (int i = 0; i < Settings::kNumWormSettings; ++i)
    settings_host->worm_settings[i] =
        std::make_shared<WormSettings>(*settings_host->worm_settings[i]);
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->color = 3;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->rgb[0] = 255;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->rgb[1] = 0;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->rgb[2] = 0;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[0] = 10;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[1] = 20;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[2] = 30;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[3] = 5;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[4] = 15;

  auto settings_client = std::make_shared<Settings>(*f.settings);
  for (int i = 0; i < Settings::kNumWormSettings; ++i)
    settings_client->worm_settings[i] =
        std::make_shared<WormSettings>(*settings_client->worm_settings[i]);
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->color = 6;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->rgb[0] = 0;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->rgb[1] = 255;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->rgb[2] = 128;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[0] = 35;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[1] = 8;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[2] = 22;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[3] = 14;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[4] = 40;

  NetSession host(f.common, settings_host, f.tc_root);
  NetSession client(f.common, settings_client, f.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  // Host's remote worm (index 1) should have client's color/weapons
  Worm* host_remote_worm = host.Rollback()->game.WormByIdx(1);
  REQUIRE(host_remote_worm->settings->color == 6);
  REQUIRE(host_remote_worm->settings->rgb[0] == 0);
  REQUIRE(host_remote_worm->settings->rgb[1] == 255);
  REQUIRE(host_remote_worm->settings->rgb[2] == 128);
  REQUIRE(host_remote_worm->settings->weapons[0] == 35);
  REQUIRE(host_remote_worm->settings->weapons[4] == 40);

  // Client's remote worm (index 0) should have host's color/weapons
  Worm* client_remote_worm = client.Rollback()->game.WormByIdx(0);
  REQUIRE(client_remote_worm->settings->color == 3);
  REQUIRE(client_remote_worm->settings->rgb[0] == 255);
  REQUIRE(client_remote_worm->settings->rgb[1] == 0);
  REQUIRE(client_remote_worm->settings->rgb[2] == 0);
  REQUIRE(client_remote_worm->settings->weapons[0] == 10);
  REQUIRE(client_remote_worm->settings->weapons[4] == 15);

  // Persistent settings should NOT be modified
  REQUIRE(settings_host->worm_settings[1]->color != 6);
  REQUIRE(settings_client->worm_settings[0]->color != 3);
}

TEST_CASE("NetSession client detects host disconnect", "[session]") {
  SessionFixture f;

  NetSession host(f.common, f.settings, f.tc_root);
  NetSession client(f.common, f.settings, f.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(ready);

  // Host disconnects
  host.Disconnect();
  REQUIRE(host.State() == NetSession::kIdle);

  // Client should detect the disconnect
  bool disconnected = false;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!disconnected && std::chrono::steady_clock::now() < deadline) {
    client.Update();
    disconnected = (client.State() == NetSession::kDisconnected);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  REQUIRE(disconnected);
}

TEST_CASE("NetSession connect to non-existent host fails", "[session]") {
  SessionFixture f;

  NetSession client(f.common, f.settings, f.tc_root);
  // Connect to a port where nobody is listening
  REQUIRE(client.JoinGame("127.0.0.1", 19599));

  // Poll — should eventually fail or stay in WaitingForPeer
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (client.State() == NetSession::kWaitingForPeer &&
         std::chrono::steady_clock::now() < deadline) {
    client.Update();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Should not be in Playing state
  REQUIRE(client.State() != NetSession::kPlaying);
}

TEST_CASE("NetSession TC sync transfers data when hashes differ", "[session][tc]") {
  SessionFixture f;

  // Create a modified TC in a temp directory so the client has a different hash.
  // Copy the real TC and append a byte to tc.cfg to change the hash.
  std::string temp_tc_dir = "/tmp/openliero_test_tc_modified";

  // Pack the real TC, unpack to temp dir, then modify a file
  auto archive = tc_archive::Pack(f.tc_root);
  auto files = tc_archive::Unpack(archive.data(), archive.size());
  REQUIRE(!files.empty());

  // Write files to temp dir
  std::filesystem::remove_all(temp_tc_dir);
  std::filesystem::create_directories(temp_tc_dir);

  for (auto& file : files) {
    std::filesystem::path full_path = std::filesystem::path(temp_tc_dir) / file.name;
    std::filesystem::create_directories(full_path.parent_path());
    std::ofstream ofs(full_path, std::ios::binary);
    REQUIRE(ofs.is_open());
    ofs.write(reinterpret_cast<const char*>(file.data.data()),
              static_cast<std::streamsize>(file.data.size()));
  }

  // Modify a sound file to change the hash without breaking TOML parsing
  {
    std::filesystem::path wav_path = std::filesystem::path(temp_tc_dir) / "sounds/shotgun.wav";
    std::ofstream ofs(wav_path, std::ios::binary | std::ios::app);
    REQUIRE(ofs.is_open());
    ofs.put('\0');
  }

  FsNode client_tc_root(temp_tc_dir);
  uint32_t client_hash = tc_archive::ComputeHash(client_tc_root);
  uint32_t host_hash = tc_archive::ComputeHash(f.tc_root);
  REQUIRE(client_hash != host_hash);  // Ensure hashes actually differ

  // Client uses the modified TC
  auto client_common = std::make_shared<Common>();
  client_common->load(client_tc_root);

  auto client_settings = std::make_shared<Settings>(*f.settings);

  // Track if onTcReloaded fires
  bool tc_reloaded = false;
  std::shared_ptr<Common> received_common;

  NetSession host(f.common, f.settings, f.tc_root);
  NetSession client(client_common, client_settings, client_tc_root);

  client.on_tc_reloaded = [&](std::shared_ptr<Common> new_common) {
    tc_reloaded = true;
    received_common = new_common;
  };

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  // Poll until both reach Playing state (TC transfer included)
  bool ready = PollUntil(
      host, client,
      [&]() {
        return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
      },
      10000);  // Allow more time for TC transfer

  INFO("Host state: " << (int)host.State());
  INFO("Client state: " << (int)client.State());
  REQUIRE(ready);

  // TC should have been reloaded on the client
  REQUIRE(tc_reloaded);
  REQUIRE(received_common != nullptr);

  // Both games should have the same RNG seed (basic sanity)
  REQUIRE(host.Rollback()->game.rand.last == client.Rollback()->game.rand.last);

  // Clean up
  std::filesystem::remove_all(temp_tc_dir);
}

TEST_CASE("NetSession TC sync skips transfer when hashes match", "[session][tc]") {
  SessionFixture f;

  // Both sides use the same TC — no transfer should happen
  bool tc_reloaded = false;

  NetSession host(f.common, f.settings, f.tc_root);
  NetSession client(f.common, f.settings, f.tc_root);

  client.on_tc_reloaded = [&](std::shared_ptr<Common>) { tc_reloaded = true; };

  REQUIRE(host.HostGame(0));
  uint16_t port = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", port));

  bool ready = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });

  REQUIRE(ready);
  // TC should NOT have been reloaded (same hash → skip)
  REQUIRE_FALSE(tc_reloaded);
}
