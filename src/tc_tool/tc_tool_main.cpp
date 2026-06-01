#include "common_exereader.hpp"
#include "common_writer.hpp"
#include "game/filesystem.hpp"
#include "game/reader.hpp"

#include <cstring>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
  // Pre-strip --tc-name <value> before passing to paths::resolve so its
  // value isn't mistaken for a positional argument.
  std::string tcName;
  std::vector<std::string> argStorage;
  std::vector<char*> argPtrs;
  argStorage.reserve(argc);
  argStorage.emplace_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--tc-name") == 0 && i + 1 < argc) {
      tcName = argv[++i];
    } else {
      argStorage.emplace_back(argv[i]);
    }
  }
  for (auto& s : argStorage) argPtrs.push_back(s.data());
  argPtrs.push_back(nullptr);

  auto r = paths::resolve(static_cast<int>(argStorage.size()), argPtrs.data());

  // First positional is the path to the legacy Liero install directory.
  if (r.positionalArgs.empty()) {
    printf("tctool <path-to-tc>\n");
    return 0;
  }
  std::string const& exePath = r.positionalArgs[0];

  Common common;

  FsNode path(exePath);

  bool found = false;

  for (auto const& name : path.iter()) {
    if (toUpperCase(name.name).find(".EXE") != std::string::npos) {
      auto exeReader_ptr = (path / name.name).toReader();
      io::Reader& exeReader = *exeReader_ptr;
      ReaderFile exe(exeReader);

      if (exe.len() >= 135000 && exe.len() <= 137000) {
        printf("Converting %s...\n", name.name.c_str());

        // TODO: Some TCs change the name of the .SND or .CHR for some reason.
        // We could read that name from the exe to make them work.
        auto gfxReader_ptr = (path / "LIERO.CHR").toReader();
        io::Reader& gfxReader = *gfxReader_ptr;
        ReaderFile gfx(gfxReader);
        auto sndReader_ptr = (path / "LIERO.SND").toReader();
        io::Reader& sndReader = *sndReader_ptr;
        ReaderFile snd(sndReader);

        loadFromExe(common, exe, gfx, snd);

        if (tcName.empty()) tcName = getLeaf(exePath);

        FsNode outNode = r.userConfigNode / "TC" / tcName;

        printf("Writing to %s...\n", outNode.fullPath().c_str());

        commonSave(common, outNode.fullPath());

        found = true;
        break;
      }
    }
  }

  if (!found) {
    printf("Could not find a suitable LIERO.EXE in %s\n", exePath.c_str());
  }

  return 0;
}
