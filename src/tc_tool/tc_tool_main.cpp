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
  std::string tc_name;
  std::vector<std::string> arg_storage;
  std::vector<char*> arg_ptrs;
  arg_storage.reserve(argc);
  arg_storage.emplace_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--tc-name") == 0 && i + 1 < argc) {
      tc_name = argv[++i];
    } else {
      arg_storage.emplace_back(argv[i]);
    }
  }
  for (auto& s : arg_storage) arg_ptrs.push_back(s.data());
  arg_ptrs.push_back(nullptr);

  auto r = paths::Resolve(static_cast<int>(arg_storage.size()), arg_ptrs.data());

  // First positional is the path to the legacy Liero install directory.
  if (r.positional_args.empty()) {
    printf("tctool <path-to-tc>\n");
    return 0;
  }
  std::string const& exe_path = r.positional_args[0];

  Common common;

  FsNode path(exe_path);

  bool found = false;

  for (auto const& name : path.Iter()) {
    if (ToUpperCase(name.name).find(".EXE") != std::string::npos) {
      auto exe_reader_ptr = (path / name.name).ToReader();
      io::Reader& exe_reader = *exe_reader_ptr;
      ReaderFile exe(exe_reader);

      if (exe.Len() >= 135000 && exe.Len() <= 137000) {
        printf("Converting %s...\n", name.name.c_str());

        // TODO: Some TCs change the name of the .SND or .CHR for some reason.
        // We could read that name from the exe to make them work.
        auto gfx_reader_ptr = (path / "LIERO.CHR").ToReader();
        io::Reader& gfx_reader = *gfx_reader_ptr;
        ReaderFile gfx(gfx_reader);
        auto snd_reader_ptr = (path / "LIERO.SND").ToReader();
        io::Reader& snd_reader = *snd_reader_ptr;
        ReaderFile snd(snd_reader);

        LoadFromExe(common, exe, gfx, snd);

        if (tc_name.empty()) tc_name = GetLeaf(exe_path);

        FsNode out_node = r.user_config_node / "TC" / tc_name;

        printf("Writing to %s...\n", out_node.FullPath().c_str());

        CommonSave(common, out_node.FullPath());

        found = true;
        break;
      }
    }
  }

  if (!found) {
    printf("Could not find a suitable LIERO.EXE in %s\n", exe_path.c_str());
  }

  return 0;
}
