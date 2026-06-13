#pragma once

#include <string>

struct VideoToolArgs {
  bool dir{false};
  bool spectator{false};
  int width{1280};
  int height{720};
  std::string tc_name{"openliero"};
  std::string replay_path;
};

inline VideoToolArgs ParseVideoToolArgs(int argc, char* argv[]) {
  VideoToolArgs args;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
        case 'd':
          args.dir = true;
          break;
        case 's':
          args.spectator = true;
          break;
        case 'r':
          ++i;
          if (i < argc) {
            args.replay_path = argv[i];
          }
          break;
        case 'w':
          ++i;
          if (i < argc) {
            args.width = std::stoi(argv[i]);
          }
          break;
        case 'h':
          ++i;
          if (i < argc) {
            args.height = std::stoi(argv[i]);
          }
          break;
        default:
          break;
      }
    } else {
      args.tc_name = argv[i];
    }
  }
  return args;
}
