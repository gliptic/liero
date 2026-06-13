#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <utility>

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
  SessionFixture const kF;

  NetSession host(kF.common, kF.settings, kF.tc_root);
  NetSession client(kF.common, kF.settings, kF.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(host.State() == NetSession::kWaitingForPeer);

  REQUIRE(client.JoinGame("127.0.0.1", kPort));
  REQUIRE(client.State() == NetSession::kWaitingForPeer);

  // Poll until both reach Playing state
  bool const kReady = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });

  REQUIRE(kReady);
  REQUIRE(host.Rollback() != nullptr);
  REQUIRE(client.Rollback() != nullptr);

  // Both games should have the same RNG seed
  REQUIRE(host.Rollback()->game.rand == client.Rollback()->game.rand);
}

TEST_CASE("NetSession syncs host settings to client", "[session]") {
  SessionFixture const kF;

  auto settings_b = std::make_shared<Settings>(*kF.settings);
  settings_b->lives = 99;
  settings_b->blood = 200;
  settings_b->loading_time = 50;
  settings_b->game_mode = Settings::kGmGameOfTag;
  settings_b->max_bonuses = 7;
  settings_b->time_to_lose = 999;
  settings_b->flags_to_win = 3;
  settings_b->load_change = true;
  // Modify some weapTable entries
  for (int i = 0; i < 40; ++i) {
    settings_b->weap_table[i] = (i < 10) ? 2 : 0;
  }

  NetSession host(kF.common, kF.settings, kF.tc_root);
  NetSession client(kF.common, settings_b, kF.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", kPort));

  // Poll until both reach Playing — host settings are authoritative
  bool const kReady = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });

  REQUIRE(kReady);
  // Client should have received and applied host's settings
  REQUIRE(settings_b->lives == kF.settings->lives);
  REQUIRE(settings_b->blood == kF.settings->blood);
  REQUIRE(settings_b->loading_time == kF.settings->loading_time);
  REQUIRE(settings_b->game_mode == kF.settings->game_mode);
  REQUIRE(settings_b->max_bonuses == kF.settings->max_bonuses);
  REQUIRE(settings_b->time_to_lose == kF.settings->time_to_lose);
  REQUIRE(settings_b->flags_to_win == kF.settings->flags_to_win);
  REQUIRE(settings_b->load_change == kF.settings->load_change);
  for (int i = 0; i < 40; ++i) {
    REQUIRE(settings_b->weap_table[i] == kF.settings->weap_table[i]);
  }
}

TEST_CASE("NetSession syncs worm colors and weapons between peers", "[session]") {
  SessionFixture const kF;

  // Give each player distinct colors and weapons in the network player slot
  auto settings_host = std::make_shared<Settings>(*kF.settings);
  // Create distinct WormSettings objects so shared_ptr sharing doesn't cause issues
  for (auto& worm_setting : settings_host->worm_settings) {
    worm_setting = std::make_shared<WormSettings>(*worm_setting);
  }
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->color = 3;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->rgb[0] = 255;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->rgb[1] = 0;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->rgb[2] = 0;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[0] = 10;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[1] = 20;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[2] = 30;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[3] = 5;
  settings_host->worm_settings[Settings::kNetworkPlayerIdx]->weapons[4] = 15;

  auto settings_client = std::make_shared<Settings>(*kF.settings);
  for (auto& worm_setting : settings_client->worm_settings) {
    worm_setting = std::make_shared<WormSettings>(*worm_setting);
  }
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->color = 6;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->rgb[0] = 0;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->rgb[1] = 255;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->rgb[2] = 128;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[0] = 35;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[1] = 8;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[2] = 22;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[3] = 14;
  settings_client->worm_settings[Settings::kNetworkPlayerIdx]->weapons[4] = 40;

  NetSession host(kF.common, settings_host, kF.tc_root);
  NetSession client(kF.common, settings_client, kF.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", kPort));

  bool const kReady = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(kReady);

  // Host's remote worm (index 1) should have client's color/weapons
  Worm const* host_remote_worm = host.Rollback()->game.WormByIdx(1);
  REQUIRE(host_remote_worm->settings->color == 6);
  REQUIRE(host_remote_worm->settings->rgb[0] == 0);
  REQUIRE(host_remote_worm->settings->rgb[1] == 255);
  REQUIRE(host_remote_worm->settings->rgb[2] == 128);
  REQUIRE(host_remote_worm->settings->weapons[0] == 35);
  REQUIRE(host_remote_worm->settings->weapons[4] == 40);

  // Client's remote worm (index 0) should have host's color/weapons
  Worm const* client_remote_worm = client.Rollback()->game.WormByIdx(0);
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
  SessionFixture const kF;

  NetSession host(kF.common, kF.settings, kF.tc_root);
  NetSession client(kF.common, kF.settings, kF.tc_root);

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", kPort));

  bool const kReady = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });
  REQUIRE(kReady);

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
  SessionFixture const kF;

  NetSession client(kF.common, kF.settings, kF.tc_root);
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
  SessionFixture const kF;

  // Create a modified TC in a temp directory so the client has a different hash.
  // Copy the real TC and append a byte to tc.cfg to change the hash.
  std::string const kTempTcDir = "/tmp/openliero_test_tc_modified";

  // Pack the real TC, unpack to temp dir, then modify a file
  auto archive = tc_archive::Pack(kF.tc_root);
  auto files = tc_archive::Unpack(archive.data(), archive.size());
  REQUIRE(!files.empty());

  // Write files to temp dir
  std::filesystem::remove_all(kTempTcDir);
  std::filesystem::create_directories(kTempTcDir);

  for (auto& file : files) {
    std::filesystem::path const kFullPath = std::filesystem::path(kTempTcDir) / file.name;
    std::filesystem::create_directories(kFullPath.parent_path());
    std::ofstream ofs(kFullPath, std::ios::binary);
    REQUIRE(ofs.is_open());
    ofs.write(reinterpret_cast<const char*>(file.data.data()),
              static_cast<std::streamsize>(file.data.size()));
  }

  // Modify a sound file to change the hash without breaking TOML parsing
  {
    std::filesystem::path const kWavPath = std::filesystem::path(kTempTcDir) / "sounds/shotgun.wav";
    std::ofstream ofs(kWavPath, std::ios::binary | std::ios::app);
    REQUIRE(ofs.is_open());
    ofs.put('\0');
  }

  FsNode const kClientTcRoot(kTempTcDir);
  uint32_t const kClientHash = tc_archive::ComputeHash(kClientTcRoot);
  uint32_t const kHostHash = tc_archive::ComputeHash(kF.tc_root);
  REQUIRE(kClientHash != kHostHash);  // Ensure hashes actually differ

  // Client uses the modified TC
  auto client_common = std::make_shared<Common>();
  client_common->load(kClientTcRoot);

  auto client_settings = std::make_shared<Settings>(*kF.settings);

  // Track if onTcReloaded fires
  bool tc_reloaded = false;
  std::shared_ptr<Common> received_common;

  NetSession host(kF.common, kF.settings, kF.tc_root);
  NetSession client(client_common, client_settings, kClientTcRoot);

  client.on_tc_reloaded = [&](std::shared_ptr<Common> new_common) {
    tc_reloaded = true;
    received_common = std::move(new_common);
  };

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", kPort));

  // Poll until both reach Playing state (TC transfer included)
  bool const kReady = PollUntil(
      host, client,
      [&]() {
        return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
      },
      10000);  // Allow more time for TC transfer

  INFO("Host state: " << (int)host.State());
  INFO("Client state: " << (int)client.State());
  REQUIRE(kReady);

  // TC should have been reloaded on the client
  REQUIRE(tc_reloaded);
  REQUIRE(received_common != nullptr);

  // Both games should have the same RNG seed (basic sanity)
  REQUIRE(host.Rollback()->game.rand.last == client.Rollback()->game.rand.last);

  // Clean up
  std::filesystem::remove_all(kTempTcDir);
}

TEST_CASE("NetSession TC sync skips transfer when hashes match", "[session][tc]") {
  SessionFixture const kF;

  // Both sides use the same TC — no transfer should happen
  bool tc_reloaded = false;

  NetSession host(kF.common, kF.settings, kF.tc_root);
  NetSession client(kF.common, kF.settings, kF.tc_root);

  client.on_tc_reloaded = [&](const std::shared_ptr<Common>&) { tc_reloaded = true; };

  REQUIRE(host.HostGame(0));
  uint16_t const kPort = host.Transport().ListeningPort();
  REQUIRE(client.JoinGame("127.0.0.1", kPort));

  bool const kReady = PollUntil(host, client, [&]() {
    return host.State() == NetSession::kPlaying && client.State() == NetSession::kPlaying;
  });

  REQUIRE(kReady);
  // TC should NOT have been reloaded (same hash → skip)
  REQUIRE_FALSE(tc_reloaded);
}

TEST_CASE("NetTransport protocol version is 8 for anim blob", "[session][anim-layer]") {
  CHECK(NetTransport::kProtocolVersion == 8);
}

TEST_CASE("level blob round-trip preserves anim layer", "[session][anim-layer]") {
  SessionFixture const kF;
  RollbackController ctrl(kF.common, kF.settings, /*local_player_idx=*/0);

  constexpr uint16_t kW = 4;
  constexpr uint16_t kH = 3;
  constexpr size_t kCells = kW * kH;

  Rand const kRng(42U);
  std::string const kRandState = kRng.serialize();
  auto const kRandStatLen = static_cast<uint32_t>(kRandState.size());
  uint32_t const kRandLast = kRng.last;

  std::vector<uint8_t> blob;
  auto push8 = [&](uint8_t v) { blob.push_back(v); };
  auto push16le = [&](uint16_t v) {
    blob.push_back(v & 0xFF);
    blob.push_back((v >> 8) & 0xFF);
  };
  auto push32le = [&](uint32_t v) {
    blob.push_back(v & 0xFF);
    blob.push_back((v >> 8) & 0xFF);
    blob.push_back((v >> 16) & 0xFF);
    blob.push_back((v >> 24) & 0xFF);
  };

  push16le(kW);
  push16le(kH);
  push32le(kRandStatLen);
  for (char const kByte : kRandState) {
    push8(static_cast<uint8_t>(kByte));
  }
  push32le(kRandLast);
  for (size_t i = 0; i < kCells; ++i) {
    push8(0);
  }  // material_id
  for (int i = 0; i < 768; ++i) {
    push8(0);
  }  // palette
  push8(1);  // has_display_layer
  for (size_t i = 0; i < kCells; ++i) {
    push32le(0xFF102030U);
  }  // display_data
  for (size_t i = 0; i < kCells; ++i) {
    push8(1);
  }  // display_valid
  // anim section
  push8(1);  // ramp_count = 1
  push8(2);  // shift
  push8(2);
  push8(0);  // color_count LE = 2
  push32le(0xFF204060U);
  push32le(0xFF608020U);
  for (size_t i = 0; i < kCells; ++i) {
    push8(i == 5 ? 1 : 0);
  }  // display_anim

  // LoadLevelFromData expects: compressed_flag(1) + raw_size(4) + raw_data
  auto const kRawSize32 = static_cast<uint32_t>(blob.size());
  std::vector<uint8_t> packet;
  packet.push_back(0);  // not compressed
  for (int bi = 0; bi < 4; ++bi) {
    packet.push_back((kRawSize32 >> (8 * bi)) & 0xFF);
  }
  packet.insert(packet.end(), blob.begin(), blob.end());

  ctrl.LoadLevelFromData(packet);

  REQUIRE(ctrl.game.level.argb_ramps.size() == 1);
  CHECK(ctrl.game.level.argb_ramps[0].shift == 2);
  REQUIRE(ctrl.game.level.argb_ramps[0].colors.size() == 2);
  CHECK(ctrl.game.level.argb_ramps[0].colors[0] == 0xFF204060U);
  CHECK(ctrl.game.level.argb_ramps[0].colors[1] == 0xFF608020U);
  REQUIRE(ctrl.game.level.display_anim.size() == kCells);
  CHECK(ctrl.game.level.display_anim[5] == 1);
  CHECK(ctrl.game.level.display_anim[0] == 0);
}
