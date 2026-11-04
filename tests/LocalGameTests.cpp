#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "gameplay/LocalGame.h"
#include "gameplay/StartingCells.h"
#include "gameplay/content/GameContent.h"
#include "world/TerrainSource.h"

namespace {
using namespace Infinitesia;

constexpr int kSize = 64;
constexpr int kHeight = 24;
constexpr int kGround = 8;

// The minimum registry the test terrains draw their materials from.
const BlockRegistry& TestRegistry() {
  static const BlockRegistry registry = BlockRegistry::BuiltIn();
  return registry;
}

MaterialId Material(const char* key) {
  return TestRegistry().FindMaterial(key)->id;
}

// Terrain made of a height function: solid up to and including height(x, z),
// air above.
class HeightTerrain final : public TerrainSource {
 public:
  explicit HeightTerrain(std::function<int(int, int)> height)
      : m_height(std::move(height)) {}

  BlockState StateAt(GridCell cell) const override {
    return {cell.y <= m_height(cell.x, cell.z) ? Material("rock.bedrock")
                                               : kAirMaterial,
            kShapeFull};
  }
  const BlockRegistry& Registry() const override { return TestRegistry(); }

 private:
  std::function<int(int, int)> m_height;
};

const Grid& TestGrid() {
  static const Grid grid(GridConfig{kSize, kHeight, kSize, 1.0f});
  return grid;
}
}  // namespace

TEST_CASE("TheWorkerStartsOnTheGroundOfTheMiddleColumn") {
  const HeightTerrain terrain([](int, int) { return kGround; });
  const StartingCells cells = FindStartingCells(TestGrid(), terrain);
  CHECK(cells.worker.x == kSize / 2);
  CHECK(cells.worker.z == kSize / 2);
  CHECK(cells.worker.y == kGround);
}

TEST_CASE("WaterIsNotGround") {
  const class Flooded final : public TerrainSource {
   public:
    BlockState StateAt(GridCell cell) const override {
      if (cell.y <= kGround) {
        return {Material("soil.sand"), kShapeFull};
      }
      return {cell.y <= kGround + 3 ? Material("fluid.water") : kAirMaterial,
              kShapeFull};
    }
    const BlockRegistry& Registry() const override { return TestRegistry(); }
  } terrain;
  CHECK(SurfaceLevelOf(TestGrid(), terrain, 10, 10) == kGround);
  CHECK(SurfaceLevelOf(TestGrid(), terrain, -1, 10) == -1);
}
