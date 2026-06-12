#include "settings.hpp"

#include "filesystem.hpp"
#include "gfx.hpp"
#include "io/stream.hpp"
#include "keys.hpp"

#include <memory>
#include <serialization/cereal_types.hpp>
#include <serialization/toml_archive.hpp>

#include <xxhash.h>
#include <sstream>

int const Settings::kWormAnimTab[] = {0, 7, 0, 14};

GameplayExtensions::GameplayExtensions() : tc(std::string("openliero")) {}

AppSettings::AppSettings()

    = default;

Settings::Settings()

{
  std::memset(weap_table, 0, sizeof(weap_table));

  worm_settings[0] = std::make_shared<WormSettings>();
  worm_settings[1] = std::make_shared<WormSettings>();
  worm_settings[2] = std::make_shared<WormSettings>();

  worm_settings[0]->color = 32;
  worm_settings[1]->color = 41;
  worm_settings[2]->color = 32;

  unsigned char const kDefControls[2][7] = {{0x13, 0x21, 0x20, 0x22, 0x1D, 0x2A, 0x38},
                                            {0xA0, 0xA8, 0xA3, 0xA5, 0x75, 0x90, 0x36}};

  unsigned char const kDefRgb[2][3] = {{104, 104, 252}, {60, 172, 60}};

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 7; ++j) {
      worm_settings[i]->controls[j] = kDefControls[i][j];
      worm_settings[i]->controls_ex[j] = kDefControls[i][j];
    }

    for (int j = 0; j < 3; ++j) {
      worm_settings[i]->rgb[j] = kDefRgb[i][j];
    }
  }

  // Network player defaults to left player's controls and color
  for (int j = 0; j < 7; ++j) {
    worm_settings[2]->controls[j] = kDefControls[0][j];
    worm_settings[2]->controls_ex[j] = kDefControls[0][j];
  }
  for (int j = 0; j < 3; ++j) {
    worm_settings[2]->rgb[j] = kDefRgb[0][j];
  }
}

bool Settings::load(const FsNode& node, Rand& /*rand*/) {
  try {
    auto reader_ptr = node.ToReader();
    io::Reader& reader = *reader_ptr;
    std::string content;
    uint8_t buf[4096];
    for (;;) {
      std::size_t const kGot = reader.TryGet(buf, sizeof(buf));
      if (kGot == 0) {
        break;
      }
      content.append(reinterpret_cast<char*>(buf), kGot);
    }
    FromToml(content);
  } catch (std::runtime_error&) {
    return false;
  }

  // Validate that wormSettings were deserialized (guards against corrupt
  // or incompatible config files that lack player sections).
  // NOLINTNEXTLINE(readability-use-anyofallof) — loop body has more than the predicate check; rewriting as std::any_of/all_of would be less readable here.
  for (auto& worm_setting : worm_settings) {
    if (!worm_setting) {
      return false;
    }
  }

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
    // TOML-only (display preference): the binary Settings blob is embedded
    // in replays and must keep its field layout.
    ar(cereal::make_nvp("modernColors", const_cast<Settings&>(*this).modern_colors));
    SerializeSettingsScalars(ar, const_cast<Settings&>(*this));
    SerializeArray(ar, "weapTable", const_cast<Settings&>(*this).weap_table);
    ar.finishNode();

    // Serialize worm settings as sub-tables with descriptive names
    static char const* const kWormNames[] = {"player1", "player2", "network_player"};
    for (int i = 0; i < kNumWormSettings; ++i) {
      if (worm_settings[i]) {
        ar.setNextName(kWormNames[i]);
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
  ar(cereal::make_nvp("modernColors", modern_colors));
  SerializeSettingsScalars(ar, *this);
  SerializeArray(ar, "weapTable", weap_table);
  ar.finishNode();

  static char const* const kWormNames[] = {"player1", "player2", "network_player"};
  for (int i = 0; i < kNumWormSettings; ++i) {
    ar.setNextName(kWormNames[i]);
    ar.startNode();
    if (!worm_settings[i]) {
      worm_settings[i] = std::make_shared<WormSettings>();
    }
    SerializeWormSettingsToml(ar, *worm_settings[i]);
    ar.finishNode();
  }
}

void Settings::save(const FsNode& node, Rand& /*rand*/) const {
  auto writer_ptr = node.ToWriter();
  io::Writer& writer = *writer_ptr;
  std::string toml = ToToml();
  writer.Put(reinterpret_cast<uint8_t const*>(toml.data()), toml.size());
}

void Settings::GenerateName(WormSettings& ws, Rand& rand) {
  // NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) — disabled block kept as a sketch of the planned NAMES.DAT loader.
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
