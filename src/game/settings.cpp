#include "settings.hpp"

#include "filesystem.hpp"
#include "gfx.hpp"
#include "io/stream.hpp"
#include "keys.hpp"

#include <serialization/cereal_types.hpp>
#include <serialization/toml_archive.hpp>

#include <xxhash.h>
#include <sstream>

int const Settings::kWormAnimTab[] = {0, 7, 0, 14};

GameplayExtensions::GameplayExtensions()
    : record_replays(true),
      load_powerlevel_palette(true),
      ai_frames(70 * 2),
      ai_mutations(2),
      ai_traces(false),
      ai_parallels(3),
      zone_timeout(30),
      select_bot_weapons(true),
      allow_viewing_spawn_point(false),
      tc(std::string("openliero")) {}

AppSettings::AppSettings()
    : fullscreen(false),
      single_screen_replay(false),
      spectator_window(false),
      blood_particle_max(700) {}

Settings::Settings()
    : max_bonuses(4),
      blood(100),
      time_to_lose(600),
      flags_to_win(20),
      game_mode(0),
      shadow(true),
      load_change(true),
      names_on_bonuses(false),
      regenerate_level(false),
      lives(15),
      loading_time(100),
      random_level(true),
      map(true),
      screen_sync(true),
      bonus_timeout(0),
      input_delay(1) {
  std::memset(weap_table, 0, sizeof(weap_table));

  worm_settings[0].reset(new WormSettings);
  worm_settings[1].reset(new WormSettings);
  worm_settings[2].reset(new WormSettings);

  worm_settings[0]->color = 32;
  worm_settings[1]->color = 41;
  worm_settings[2]->color = 32;

  unsigned char def_controls[2][7] = {{0x13, 0x21, 0x20, 0x22, 0x1D, 0x2A, 0x38},
                                      {0xA0, 0xA8, 0xA3, 0xA5, 0x75, 0x90, 0x36}};

  unsigned char def_rgb[2][3] = {{26, 26, 63}, {15, 43, 15}};

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 7; ++j) {
      worm_settings[i]->controls[j] = def_controls[i][j];
      worm_settings[i]->controls_ex[j] = def_controls[i][j];
    }

    for (int j = 0; j < 3; ++j) {
      worm_settings[i]->rgb[j] = def_rgb[i][j];
    }
  }

  // Network player defaults to left player's controls and color
  for (int j = 0; j < 7; ++j) {
    worm_settings[2]->controls[j] = def_controls[0][j];
    worm_settings[2]->controls_ex[j] = def_controls[0][j];
  }
  for (int j = 0; j < 3; ++j) {
    worm_settings[2]->rgb[j] = def_rgb[0][j];
  }
}

bool Settings::load(FsNode node, Rand& rand) {
  try {
    auto reader_ptr = node.ToReader();
    io::Reader& reader = *reader_ptr;
    std::string content;
    uint8_t buf[4096];
    for (;;) {
      std::size_t got = reader.TryGet(buf, sizeof(buf));
      if (got == 0) break;
      content.append(reinterpret_cast<char*>(buf), got);
    }
    FromToml(content);
  } catch (std::runtime_error&) {
    return false;
  }

  // Validate that wormSettings were deserialized (guards against corrupt
  // or incompatible config files that lack player sections).
  for (int i = 0; i < kNumWormSettings; ++i)
    if (!worm_settings[i]) return false;

  return true;
}

uint64_t& Settings::UpdateHash() {
  std::ostringstream ss;
  {
    cereal::TomlOutputArchive ar(ss);
    SerializeGameplay(ar, *this);
  }
  std::string buf = ss.str();
  hash = XXH3_64bits(buf.data(), buf.size());
  return hash;
}

std::string Settings::ToToml() const {
  std::ostringstream ss;
  {
    cereal::TomlOutputArchive ar(ss);
    // Serialize all scalar fields under [settings]
    ar.setNextName("settings");
    ar.startNode();
    int32_t version = kConfigVersion;
    ar(cereal::make_nvp("version", version));
    SerializeSettingsScalars(ar, const_cast<Settings&>(*this));
    SerializeArray(ar, "weapTable", const_cast<Settings&>(*this).weap_table);
    ar.finishNode();

    // Serialize worm settings as sub-tables with descriptive names
    static const char* worm_names[] = {"player1", "player2", "network_player"};
    for (int i = 0; i < kNumWormSettings; ++i) {
      if (worm_settings[i]) {
        ar.setNextName(worm_names[i]);
        ar.startNode();
        SerializeWormSettingsToml(ar, *worm_settings[i]);
        ar.finishNode();
      }
    }
  }
  return ss.str();
}

void Settings::FromToml(std::string const& data) {
  std::istringstream ss(data);
  cereal::TomlInputArchive ar(ss);

  ar.setNextName("settings");
  ar.startNode();
  int32_t version = 0;
  ar(cereal::make_nvp("version", version));
  SerializeSettingsScalars(ar, *this);
  SerializeArray(ar, "weapTable", weap_table);
  ar.finishNode();

  static const char* worm_names[] = {"player1", "player2", "network_player"};
  for (int i = 0; i < kNumWormSettings; ++i) {
    ar.setNextName(worm_names[i]);
    ar.startNode();
    if (!worm_settings[i]) worm_settings[i] = std::make_shared<WormSettings>();
    SerializeWormSettingsToml(ar, *worm_settings[i]);
    ar.finishNode();
  }
}

void Settings::save(FsNode node, Rand& rand) {
  auto writer_ptr = node.ToWriter();
  io::Writer& writer = *writer_ptr;
  std::string toml = ToToml();
  writer.Put(reinterpret_cast<uint8_t const*>(toml.data()), toml.size());
}

void Settings::GenerateName(WormSettings& ws, Rand& rand) {
#if 0  // TODO
	try
	{
		ReaderFile f(FsNode(joinPath(configRoot, "NAMES.DAT")).read());

		std::vector<std::string> names;

		std::size_t len = f.len;

		// TODO: This is a bit silly since we switched to ReaderFile
		std::vector<char> chars(len);

		f.get(reinterpret_cast<uint8_t*>(&chars[0]), len);

		std::size_t begin = 0;
		for(std::size_t i = 0; i < len; ++i)
		{
			if(chars[i] == '\r'
			|| chars[i] == '\n')
			{
				if(i > begin)
				{
					names.push_back(std::string(chars.begin() + begin, chars.begin() + i));
				}

				begin = i + 1;
			}
		}

		if(!names.empty())
		{
			ws.name = names[rand(uint32_t(names.size()))];
			ws.randomName = true;
		}
	}
	catch (std::runtime_error)
	{
		return;
	}
#endif
}
