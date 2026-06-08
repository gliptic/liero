#include "game/console.hpp"
#include "game/constants.hpp"
#include "game/filesystem.hpp"
#include "game/math.hpp"
#include "game/reader.hpp"
#include "game/text.hpp"

#include <cstdio>
#include <ctime>
#include <exception>
#include <memory>

#include "replay_to_video.hpp"

// NOLINTNEXTLINE(misc-no-recursion) — small glob matcher; recursion depth bounded by pattern length.
bool Match(unsigned char const* str, unsigned char const* pat) {
  if (*pat == '*') {
    return Match(str, pat + 1) || Match(str + 1, pat);
  }
  if (!*str) {
    return !*pat;
  }
  return (toupper(*str) == toupper(*pat) || *pat == '?') && Match(str + 1, pat + 1);
}

bool Match(std::string const& str, std::string const& pat) {
  return Match(reinterpret_cast<unsigned char const*>(str.c_str()),
               reinterpret_cast<unsigned char const*>(pat.c_str()));
}

int main(int argc, char* argv[]) try {
  bool tc_set = false;
  bool dir = false;
  bool spectator = false;

  std::string tc_name;
  std::string replay_path;

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
        case 'd':
          dir = true;
          break;

        case 's':
          spectator = true;
          break;

        case 'r':
          ++i;
          if (i < argc) {
            replay_path = &argv[i][0];
          }
          break;
        default:
          break;
      }
    } else {
      tc_name = argv[i];
      tc_set = true;
    }
  }

  if (!tc_set) {
    tc_name = "openliero";
  }

  PrecomputeTables();

  // Use the same path-resolution logic as the main binary.
  // paths::Resolve ignores single-dash flags, so -d/-s/-r pass through harmlessly.
  // Output videos land next to the replay file; no writes go to any config path.
  auto r = paths::Resolve(argc, argv);
  std::shared_ptr<Common> const kCommon = std::make_shared<Common>();
  kCommon->load(r.config_node / "TC" / tc_name);

  std::string suffix = "_n";
  if (spectator) {
    suffix = "_s";
  }

  if (dir) {
    auto const& root = GetRoot(replay_path);
    DirectoryListing di(root);

    for (auto const& path : di) {
      if (GetExtension(path.name) == "lrp") {
        auto const& full_path = JoinPath(root, path.name);
        if (Match(full_path, replay_path)) {
          std::printf("Converting %s\n", full_path.c_str());
          ReplayToVideo(kCommon, spectator, full_path, full_path + suffix + ".mp4");
        }
      }
    }
  } else {
    ReplayToVideo(kCommon, spectator, replay_path, replay_path + suffix + ".mp4");
  }

  return 0;
} catch (std::exception& ex) {
  console::WriteLine(std::string("EXCEPTION: ") + ex.what());
  return 1;
}
