#include <catch2/catch_test_macros.hpp>
#include <memory>

#include "ai/dijkstra.hpp"
#include "common.hpp"
#include "level.hpp"
#include "math.hpp"

static void FillBackground(Level& level, Common& common) {
  for (int y = 0; y < level.height; ++y) {
    for (int x = 0; x < level.width; ++x) {
      level.SetPixel(x, y, 0, common);
    }
  }
}

TEST_CASE("DijkstraLevel CellFromPx clamps to actual level bounds, not hardcoded 504x350",
          "[map_size]") {
  PrecomputeTables();
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  Level level(*common);
  level.Resize(252, 175);
  FillBackground(level, *common);

  DijkstraLevel dlevel;
  dlevel.Build(level, *common);

  // Pixel far outside the 252x175 level must clamp to within level bounds.
  // Before PR1: CellFromPx clamps to kFullWidth-1=503, kFullHeight-1=349
  // (hardcoded), so CoordsLevel returns (502, 350) — both outside 252x175.
  LevelCell* cell = dlevel.CellFromPx(9999, 9999);
  IVec2 const kCoords = dlevel.CoordsLevel(cell);

  REQUIRE(kCoords.x < 252);
  REQUIRE(kCoords.y < 175);
}

TEST_CASE("DijkstraLevel CellFromPx works correctly at standard 504x350 size", "[map_size]") {
  PrecomputeTables();
  auto common = std::make_shared<Common>();
  FsNode const kTcRoot(FsNode("data") / "TC" / "openliero");
  common->load(kTcRoot);

  Level level(*common);
  level.Resize(504, 350);
  FillBackground(level, *common);

  DijkstraLevel dlevel;
  dlevel.Build(level, *common);

  // Interior position should round-trip through cell coords and stay in-bounds.
  LevelCell* cell = dlevel.CellFromPx(200, 175);
  IVec2 const kCoords = dlevel.CoordsLevel(cell);

  REQUIRE(kCoords.x < 504);
  REQUIRE(kCoords.y < 350);
}
