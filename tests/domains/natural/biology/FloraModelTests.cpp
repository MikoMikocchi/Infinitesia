#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "domains/natural/biology/community/Canopy.h"
#include "domains/natural/biology/flora/FloraModel.h"
#include "support/MatterFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
constexpr int kSize = 96;
constexpr int kGround = 100;

std::shared_ptr<const SpeciesCatalog> Catalog() {
  static const auto catalog =
      std::make_shared<const SpeciesCatalog>(SpeciesCatalog::Load(
          kAssets / "biology" / "species.json", *SharedMatter()));
  return catalog;
}

Conditions Temperate() {
  Conditions conditions;
  conditions.temperatureCelsius = 11.0;
  conditions.precipitationMm = 900.0;
  conditions.elevationMeters = 200.0;
  conditions.drySeasonMonths = 1.0;
  conditions.soilFertility = 0.7;
  conditions.soilDepth = 0.6;
  conditions.slope = 0.1;
  return conditions;
}

// Flat ground with one climate everywhere, and perhaps one vegetation sample
// from a planet layer.
class FlatGround final : public GroundView {
 public:
  explicit FlatGround(Conditions conditions, bool growable = true,
                      std::optional<VegetationSample> vegetation = std::nullopt)
      : m_conditions(conditions),
        m_growable(growable),
        m_vegetation(vegetation) {}
  GroundColumn ColumnAt(int, int) const override {
    return {m_growable, kGround, m_conditions, m_vegetation};
  }

 private:
  Conditions m_conditions;
  bool m_growable;
  std::optional<VegetationSample> m_vegetation;
};

std::uint16_t IndexOf(const char* key) {
  const auto& all = Catalog()->All();
  for (std::size_t i = 0; i < all.size(); ++i) {
    if (all[i].key == key) return static_cast<std::uint16_t>(i);
  }
  FAIL(key);
  return kNoSpecies;
}

VegetationSample Sample(
    double canopy,
    std::initializer_list<std::pair<const char*, double>> kinds) {
  VegetationSample sample;
  sample.canopyCover = canopy;
  for (const auto& [key, weight] : kinds)
    sample.entries[static_cast<std::size_t>(sample.count++)] = {IndexOf(key),
                                                                weight};
  return sample;
}

int CountTrees(const FloraModel& model) {
  int trees = 0;
  for (int sz = 0; sz < kSize / FloraModel::kSeedCellMeters; ++sz) {
    for (int sx = 0; sx < kSize / FloraModel::kSeedCellMeters; ++sx)
      trees += model.TreeOfSeedCell(sx, sz).has_value() ? 1 : 0;
  }
  return trees;
}

int SeedCells() {
  const int side = kSize / FloraModel::kSeedCellMeters;
  return side * side;
}

std::set<std::string> KindsOf(const FloraModel& model) {
  std::set<std::string> kinds;
  for (int sz = 0; sz < kSize / FloraModel::kSeedCellMeters; ++sz) {
    for (int sx = 0; sx < kSize / FloraModel::kSeedCellMeters; ++sx) {
      if (const auto tree = model.TreeOfSeedCell(sx, sz))
        kinds.insert(tree->species->key);
    }
  }
  return kinds;
}
}  // namespace

TEST_CASE("CanopyFollowsTheClimate") {
  Conditions forest = Temperate();
  CHECK(CanopyCover(forest) > 0.6);

  Conditions desert = Temperate();
  desert.precipitationMm = 60.0;
  CHECK(CanopyCover(desert) < 0.1);

  Conditions arctic = Temperate();
  arctic.temperatureCelsius = -25.0;
  CHECK(CanopyCover(arctic) < 0.1);

  Conditions savanna = Temperate();
  savanna.temperatureCelsius = 25.0;
  savanna.drySeasonMonths = 6.0;
  CHECK(CanopyCover(savanna) < CanopyCover(forest));

  Conditions barren = Temperate();
  barren.soilFertility = 0.0;
  barren.soilDepth = 0.0;
  CHECK(CanopyCover(barren) < CanopyCover(forest) * 0.6);

  for (double rain = 0.0; rain <= 4000.0; rain += 400.0) {
    Conditions conditions = Temperate();
    conditions.precipitationMm = rain;
    const double cover = CanopyCover(conditions);
    Require(cover >= 0.0 && cover <= 1.0, "canopy cover must be within 0..1");
  }
}

TEST_CASE("TheSameSeedGrowsTheSameForestInAnyOrder") {
  const FlatGround ground(Temperate());
  const FloraModel first(Catalog(), 77, kSize, kSize, ground);
  const FloraModel second(Catalog(), 77, kSize, kSize, ground);

  // The first model is asked column by column, the second back to front: chunks
  // may be built in any order.
  std::vector<std::vector<FloraCell>> forward;
  for (int z = 0; z < kSize; ++z) {
    for (int x = 0; x < kSize; ++x) forward.push_back(first.Column(x, z));
  }
  std::size_t index = forward.size();
  bool same = true;
  for (int z = kSize - 1; z >= 0; --z) {
    for (int x = kSize - 1; x >= 0; --x) {
      const std::vector<FloraCell>& other = second.Column(x, z);
      const std::vector<FloraCell>& mine = forward[--index];
      same = same && other.size() == mine.size();
      for (std::size_t i = 0; same && i < other.size(); ++i)
        same = other[i].level == mine[i].level &&
               other[i].matter == mine[i].matter;
    }
  }
  CHECK(same);

  const FloraModel other(Catalog(), 78, kSize, kSize, ground);
  CHECK(CountTrees(first) > 0);
  bool differs = false;
  for (int sz = 0; sz < kSize / FloraModel::kSeedCellMeters && !differs; ++sz) {
    for (int sx = 0; sx < kSize / FloraModel::kSeedCellMeters; ++sx) {
      const auto a = first.TreeOfSeedCell(sx, sz);
      const auto b = other.TreeOfSeedCell(sx, sz);
      differs = differs || a.has_value() != b.has_value() ||
                (a && (a->baseX != b->baseX || a->baseZ != b->baseZ ||
                       a->species != b->species));
    }
  }
  CHECK(differs);
}

TEST_CASE("ForestsFollowTheClimate") {
  const FlatGround temperateGround(Temperate());
  const FloraModel temperate(Catalog(), 5, kSize, kSize, temperateGround);
  CHECK(CountTrees(temperate) > SeedCells() * 0.2);
  CHECK(KindsOf(temperate).contains("species.oak"));
  CHECK(!KindsOf(temperate).contains("species.palm"));

  Conditions dry = Temperate();
  dry.precipitationMm = 60.0;
  const FlatGround desertGround(dry);
  CHECK(CountTrees(FloraModel(Catalog(), 5, kSize, kSize, desertGround)) == 0);

  Conditions cold = Temperate();
  cold.temperatureCelsius = 1.0;
  const FlatGround borealGround(cold);
  const FloraModel boreal(Catalog(), 5, kSize, kSize, borealGround);
  CHECK(KindsOf(boreal) == std::set<std::string>{"species.spruce"});

  Conditions hot = Temperate();
  hot.temperatureCelsius = 27.0;
  hot.precipitationMm = 2200.0;
  hot.elevationMeters = 40.0;
  const FlatGround tropicalGround(hot);
  const FloraModel tropical(Catalog(), 5, kSize, kSize, tropicalGround);
  CHECK(KindsOf(tropical) == std::set<std::string>{"species.palm"});
}

TEST_CASE("NothingGrowsWhereTheGroundCannotHoldRoots") {
  const FlatGround bare(Temperate(), false);
  const FloraModel model(Catalog(), 5, kSize, kSize, bare);
  CHECK(CountTrees(model) == 0);
  for (int z = 0; z < kSize; z += 5) {
    for (int x = 0; x < kSize; x += 5)
      Require(model.Column(x, z).empty(), "a column of bare ground has plants");
  }

  Conditions cliff = Temperate();
  cliff.slope = 2.0;
  const FlatGround steep(cliff);
  CHECK(CountTrees(FloraModel(Catalog(), 5, kSize, kSize, steep)) == 0);
}

TEST_CASE("ATreeStandsOnItsGroundWithRootsBelow") {
  const FlatGround ground(Temperate());
  const FloraModel model(Catalog(), 9, kSize, kSize, ground);
  int checked = 0;
  for (int sz = 0; sz < kSize / FloraModel::kSeedCellMeters; ++sz) {
    for (int sx = 0; sx < kSize / FloraModel::kSeedCellMeters; ++sx) {
      const auto tree = model.TreeOfSeedCell(sx, sz);
      if (!tree) continue;
      Require(tree->baseX / FloraModel::kSeedCellMeters == sx &&
                  tree->baseZ / FloraModel::kSeedCellMeters == sz,
              "a tree stands outside its seed cell");
      Require(tree->baseY == kGround + 1,
              "a trunk does not start on the ground");
      const std::vector<FloraCell>& column =
          model.Column(tree->baseX, tree->baseZ);
      const auto trunk = std::find_if(
          column.begin(), column.end(),
          [&](const FloraCell& cell) { return cell.level == tree->baseY; });
      RequireFatal(
          trunk != column.end() && trunk->part == TreePart::Trunk,
          "the first cell of a trunk is missing at the base of the tree");
      Require(trunk->matter == tree->species->matter.trunk,
              "a trunk is of the wrong material");
      ++checked;
    }
  }
  CHECK(checked > 10);
}

TEST_CASE("ColumnsHoldOneSortedCellPerLevel") {
  const FlatGround ground(Temperate());
  const FloraModel model(Catalog(), 3, kSize, kSize, ground);
  for (int z = 0; z < kSize; z += 3) {
    for (int x = 0; x < kSize; x += 3) {
      const std::vector<FloraCell>& column = model.Column(x, z);
      for (std::size_t i = 0; i < column.size(); ++i) {
        Require(i == 0 || column[i - 1].level < column[i].level,
                "levels are not strictly increasing");
        Require(column[i].part == TreePart::Root ? column[i].level <= kGround
                                                 : column[i].level > kGround,
                "only roots are in the ground");
      }
    }
  }
  CHECK(model.Column(-1, 0).empty());
  CHECK(model.Column(0, kSize).empty());
}

TEST_CASE("TheReachCoversEveryTreeOfTheCatalog") {
  const FlatGround ground(Temperate());
  const FloraModel model(Catalog(), 1, kSize, kSize, ground);
  for (const Species& species : Catalog()->All()) {
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
      const TreeVoxels tree = GrowTree(species, seed);
      const int wide = std::max({-tree.minX, tree.maxX, -tree.minZ, tree.maxZ});
      Require(wide <= model.ReachMeters(),
              species.key << " grows beyond the reach of the flora");
    }
  }
}

TEST_CASE("TheFloraNeedsACatalogAndAGrid") {
  const FlatGround ground(Temperate());
  CHECK_THROWS_AS(FloraModel(nullptr, 1, kSize, kSize, ground),
                  std::invalid_argument);
  CHECK_THROWS_AS(FloraModel(Catalog(), 1, 0, kSize, ground),
                  std::invalid_argument);
}

TEST_CASE("TheVegetationLayerSetsTheDensityAndTheKinds") {
  // The climate would grow oaks and spruces; the layer says a sparse forest of
  // spruce and oak.
  const FlatGround sparse(
      Temperate(), true,
      Sample(0.15, {{"species.spruce", 0.5}, {"species.oak", 0.5}}));
  const FlatGround dense(
      Temperate(), true,
      Sample(1.0, {{"species.spruce", 0.5}, {"species.oak", 0.5}}));
  const FloraModel sparseForest(Catalog(), 5, kSize, kSize, sparse);
  const FloraModel denseForest(Catalog(), 5, kSize, kSize, dense);
  CHECK(CountTrees(sparseForest) < CountTrees(denseForest));
  CHECK(CountTrees(sparseForest) < SeedCells() * 0.15);
  CHECK(KindsOf(denseForest) ==
        std::set<std::string>{"species.oak", "species.spruce"});

  // A treeless sample means a treeless place whatever the climate.
  const FlatGround treeless(Temperate(), true, Sample(0.0, {}));
  CHECK(CountTrees(FloraModel(Catalog(), 5, kSize, kSize, treeless)) == 0);

  // The layer's kinds still have to suit the column: palms do not grow in a
  // temperate climate.
  const FlatGround wrong(Temperate(), true,
                         Sample(1.0, {{"species.palm", 1.0}}));
  CHECK(CountTrees(FloraModel(Catalog(), 5, kSize, kSize, wrong)) == 0);

  // A forest of one kind is of that kind.
  const FlatGround oaks(Temperate(), true, Sample(1.0, {{"species.oak", 1.0}}));
  CHECK(KindsOf(FloraModel(Catalog(), 5, kSize, kSize, oaks)) ==
        std::set<std::string>{"species.oak"});
}
