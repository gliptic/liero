#include <catch2/catch_test_macros.hpp>
#include "video_tool/videotool_args.hpp"

TEST_CASE("ParseVideoToolArgs defaults") {
  char prog[] = "videotool";
  char* argv[] = {prog};
  auto args = ParseVideoToolArgs(1, argv);
  CHECK(args.width == 1280);
  CHECK(args.height == 720);
  CHECK(!args.spectator);
  CHECK(!args.dir);
  CHECK(args.tc_name == "openliero");
  CHECK(args.replay_path.empty());
}

TEST_CASE("ParseVideoToolArgs -w/-h set output resolution") {
  char prog[] = "videotool";
  char fw[] = "-w";
  char vw[] = "1920";
  char fh[] = "-h";
  char vh[] = "1080";
  char* argv[] = {prog, fw, vw, fh, vh};
  auto args = ParseVideoToolArgs(5, argv);
  CHECK(args.width == 1920);
  CHECK(args.height == 1080);
}

TEST_CASE("ParseVideoToolArgs -s sets spectator") {
  char prog[] = "videotool";
  char fs[] = "-s";
  char* argv[] = {prog, fs};
  auto args = ParseVideoToolArgs(2, argv);
  CHECK(args.spectator);
  CHECK(!args.dir);
  CHECK(args.width == 1280);
  CHECK(args.height == 720);
}

TEST_CASE("ParseVideoToolArgs -r sets replay path") {
  char prog[] = "videotool";
  char fr[] = "-r";
  char vr[] = "/path/to/replay.lrp";
  char* argv[] = {prog, fr, vr};
  auto args = ParseVideoToolArgs(3, argv);
  CHECK(args.replay_path == "/path/to/replay.lrp");
}

TEST_CASE("ParseVideoToolArgs positional arg sets tc name") {
  char prog[] = "videotool";
  char tc[] = "mytc";
  char* argv[] = {prog, tc};
  auto args = ParseVideoToolArgs(2, argv);
  CHECK(args.tc_name == "mytc");
}

TEST_CASE("ParseVideoToolArgs combined flags and resolution") {
  char prog[] = "videotool";
  char fs[] = "-s";
  char fw[] = "-w";
  char vw[] = "3840";
  char fh[] = "-h";
  char vh[] = "2160";
  char fr[] = "-r";
  char vr[] = "game.lrp";
  char* argv[] = {prog, fs, fw, vw, fh, vh, fr, vr};
  auto args = ParseVideoToolArgs(8, argv);
  CHECK(args.spectator);
  CHECK(args.width == 3840);
  CHECK(args.height == 2160);
  CHECK(args.replay_path == "game.lrp");
  CHECK(!args.dir);
}

TEST_CASE("ParseVideoToolArgs -d sets directory mode") {
  char prog[] = "videotool";
  char fd[] = "-d";
  char* argv[] = {prog, fd};
  auto args = ParseVideoToolArgs(2, argv);
  CHECK(args.dir);
}
