#include "game/console.hpp"
#include "game/constants.hpp"
#include "game/filesystem.hpp"
#include "game/math.hpp"
#include "game/reader.hpp"
#include "game/text.hpp"

#include <ctime>
#include <exception>

#include "replay_to_video.hpp"

bool match(unsigned char const* str, unsigned char const* pat) {
  if (*pat == '*') return match(str, pat + 1) || match(str + 1, pat);
  if (!*str) return !*pat;
  return (toupper(*str) == toupper(*pat) || *pat == '?') && match(str + 1, pat + 1);
}

bool match(std::string const& str, std::string const& pat) {
  return match((unsigned char const*)str.c_str(), (unsigned char const*)pat.c_str());
}

int main(int argc, char* argv[]) try {
  bool tcSet = false, dir = false, spectator = false;

  std::string tcName;
  std::string replayPath;

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
          if (i < argc) replayPath = &argv[i][0];
          break;
      }
    } else {
      tcName = argv[i];
      tcSet = true;
    }
  }

  if (!tcSet) {
    tcName = "openliero";
  }

  PrecomputeTables();

  // Use the same path-resolution logic as the main binary.
  // paths::Resolve ignores single-dash flags, so -d/-s/-r pass through harmlessly.
  // Output videos land next to the replay file; no writes go to any config path.
  auto r = paths::Resolve(argc, argv);
  std::shared_ptr<Common> common(new Common());
  common->load(r.config_node / "TC" / tcName);

  std::string suffix = "_n";
  if (spectator) {
    suffix = "_s";
  }

  if (dir) {
    auto const& root = GetRoot(replayPath);
    DirectoryListing di(root);

    for (auto const& path : di) {
      if (GetExtension(path.name) == "lrp") {
        auto const& fullPath = JoinPath(root, path.name);
        if (match(fullPath, replayPath)) {
          printf("Converting %s\n", fullPath.c_str());
          replayToVideo(common, spectator, fullPath, fullPath + suffix + ".mp4");
        }
      }
    }
  } else {
    replayToVideo(common, spectator, replayPath, replayPath + suffix + ".mp4");
  }

  return 0;
} catch (std::exception& ex) {
  console::WriteLine(std::string("EXCEPTION: ") + ex.what());
  return 1;
}
