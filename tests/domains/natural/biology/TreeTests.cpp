#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/flora/Tree.h"
#include "support/MatterFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
constexpr int kSeedsPerSpecies = 60;

const SpeciesCatalog& Catalog() {
  static const SpeciesCatalog catalog = SpeciesCatalog::Load(
      kAssets / "biology" / "species.json", *SharedMatter());
  return catalog;
}

const Species& SpeciesNamed(const char* key) {
  const Species* species = Catalog().Find(key);
  REQUIRE_MESSAGE(species != nullptr, key);
  return *species;
}

std::uint64_t Key(int x, int y, int z) {
  return static_cast<std::uint64_t>(x + 1024) |
         static_cast<std::uint64_t>(y + 1024) << 16 |
         static_cast<std::uint64_t>(z + 1024) << 32;
}

using CellMap = std::map<std::uint64_t, TreePart>;

CellMap Cells(const TreeVoxels& tree) {
  CellMap cells;
  for (const TreeVoxel& voxel : tree.cells)
    cells[Key(voxel.x, voxel.y, voxel.z)] = voxel.part;
  return cells;
}

// Steps from the cells in `from` through the cells accepted by `passable`,
// 26-connected, up to `limit`. Returns the distance of every reached cell.
std::map<std::uint64_t, int> Spread(const TreeVoxels& tree,
                                    const CellMap& cells,
                                    bool (*from)(TreePart),
                                    bool (*passable)(TreePart), int limit) {
  std::map<std::uint64_t, int> distance;
  std::deque<TreeVoxel> queue;
  for (const TreeVoxel& voxel : tree.cells) {
    if (from(voxel.part)) {
      distance[Key(voxel.x, voxel.y, voxel.z)] = 0;
      queue.push_back(voxel);
    }
  }
  while (!queue.empty()) {
    const TreeVoxel current = queue.front();
    queue.pop_front();
    const int here = distance[Key(current.x, current.y, current.z)];
    if (here >= limit) continue;
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int x = current.x + dx;
          const int y = current.y + dy;
          const int z = current.z + dz;
          const auto cell = cells.find(Key(x, y, z));
          if (cell == cells.end() || !passable(cell->second) ||
              distance.contains(cell->first))
            continue;
          distance[cell->first] = here + 1;
          queue.push_back({static_cast<std::int16_t>(x),
                           static_cast<std::int16_t>(y),
                           static_cast<std::int16_t>(z), cell->second});
        }
      }
    }
  }
  return distance;
}

bool OnlyBase(TreePart part) { return part == TreePart::Trunk; }
bool AnyWood(TreePart part) { return IsWood(part); }
bool AnyPart(TreePart) { return true; }

int Count(const TreeVoxels& tree, TreePart part) {
  return static_cast<int>(std::count_if(
      tree.cells.begin(), tree.cells.end(),
      [&](const TreeVoxel& voxel) { return voxel.part == part; }));
}

int WoodCount(const TreeVoxels& tree) {
  return static_cast<int>(
      std::count_if(tree.cells.begin(), tree.cells.end(),
                    [](const TreeVoxel& voxel) { return IsWood(voxel.part); }));
}

// The widest reach of the foliage from the axis in the cells with height in
// [from, to).
double LeafReach(const TreeVoxels& tree, double from, double to) {
  double reach = 0.0;
  for (const TreeVoxel& voxel : tree.cells) {
    if (voxel.part == TreePart::Leaves && voxel.y >= from && voxel.y < to)
      reach = std::max(reach, std::sqrt(static_cast<double>(
                                  voxel.x * voxel.x + voxel.z * voxel.z)));
  }
  return reach;
}
}  // namespace

TEST_CASE("ATreeGrowsTheSameFromTheSameSeed") {
  for (const Species& species : Catalog().All()) {
    const TreeVoxels first = GrowTree(species, 12345);
    const TreeVoxels second = GrowTree(species, 12345);
    REQUIRE(first.cells.size() == second.cells.size());
    bool same = true;
    for (std::size_t i = 0; i < first.cells.size(); ++i) {
      same = same && first.cells[i].x == second.cells[i].x &&
             first.cells[i].y == second.cells[i].y &&
             first.cells[i].z == second.cells[i].z &&
             first.cells[i].part == second.cells[i].part;
    }
    CHECK_MESSAGE(same, species.key);
  }
}

TEST_CASE("DifferentSeedsGrowDifferentTrees") {
  for (const Species& species : Catalog().All()) {
    std::size_t different = 0;
    const TreeVoxels reference = GrowTree(species, 1);
    for (std::uint64_t seed = 2; seed < 12; ++seed) {
      const TreeVoxels other = GrowTree(species, seed);
      if (other.cells.size() != reference.cells.size() ||
          other.maxY != reference.maxY)
        ++different;
    }
    CHECK_MESSAGE(different >= 8, species.key);
  }
}

TEST_CASE("CellsAreSortedAndUnique") {
  const TreeVoxels tree = GrowTree(SpeciesNamed("species.oak"), 7);
  REQUIRE(!tree.cells.empty());
  for (std::size_t i = 1; i < tree.cells.size(); ++i) {
    const TreeVoxel& a = tree.cells[i - 1];
    const TreeVoxel& b = tree.cells[i];
    const bool ordered =
        a.y < b.y || (a.y == b.y && (a.z < b.z || (a.z == b.z && a.x < b.x)));
    Require(ordered, "cells must be sorted by (y, z, x) without repeats");
  }
}

TEST_CASE("TreesStandOnTheGroundWithRootsBelow") {
  for (const Species& species : Catalog().All()) {
    for (std::uint64_t seed = 0; seed < kSeedsPerSpecies; ++seed) {
      const TreeVoxels tree = GrowTree(species, seed);
      for (const TreeVoxel& voxel : tree.cells) {
        Require(voxel.part == TreePart::Root ? voxel.y < 0 : voxel.y >= 0,
                "only roots lie below the ground");
        Require(voxel.y >= -species.rootDepthMeters,
                "a root is deeper than the species allows");
      }
      const CellMap cells = Cells(tree);
      const auto base = cells.find(Key(0, 0, 0));
      RequireFatal(base != cells.end() && base->second == TreePart::Trunk,
                   "the trunk must start at the origin");
    }
  }
}

TEST_CASE("TreesAreAsTallAndWideAsTheirSpecies") {
  for (const Species& species : Catalog().All()) {
    for (std::uint64_t seed = 0; seed < kSeedsPerSpecies; ++seed) {
      const TreeVoxels tree = GrowTree(species, seed);
      Require(tree.maxY >= species.heightMeters.min * 0.8 - 1,
              species.key << " is too short");
      Require(tree.maxY <= species.heightMeters.max + 5,
              species.key << " is too tall");
      const double wide = species.crownRadiusMeters.max + 8.0;
      Require(std::max({-tree.minX, tree.maxX, -tree.minZ, tree.maxZ}) <= wide,
              species.key << " is too wide");
      Require(tree.cells.size() < 20000, species.key << " has too many cells");
    }
  }
}

TEST_CASE("AllWoodOfATreeHoldsTogether") {
  for (const Species& species : Catalog().All()) {
    for (std::uint64_t seed = 0; seed < kSeedsPerSpecies; ++seed) {
      const TreeVoxels tree = GrowTree(species, seed);
      const CellMap cells = Cells(tree);
      // Starting from the trunk, 26-connected wood reaches all the wood.
      const auto reached = Spread(tree, cells, OnlyBase, AnyWood, 1000);
      Require(static_cast<int>(reached.size()) >= WoodCount(tree),
              species.key << " has wood that is not joined to the trunk");
    }
  }
}

TEST_CASE("EveryLeafIsNearWood") {
  constexpr int kSupportDistance = 6;
  for (const Species& species : Catalog().All()) {
    for (std::uint64_t seed = 0; seed < kSeedsPerSpecies; ++seed) {
      const TreeVoxels tree = GrowTree(species, seed);
      const CellMap cells = Cells(tree);
      const auto reached =
          Spread(tree, cells, AnyWood, AnyPart, kSupportDistance);
      Require(reached.size() == tree.cells.size(),
              species.key << " has foliage out of reach of the wood");
    }
  }
}

TEST_CASE("TreesAreThickAtTheBaseAndLeaveFewSingleCells") {
  const Species& oak = SpeciesNamed("species.oak");
  for (std::uint64_t seed = 0; seed < kSeedsPerSpecies; ++seed) {
    const TreeVoxels tree = GrowTree(oak, seed);
    int baseCells = 0;
    for (const TreeVoxel& voxel : tree.cells)
      baseCells += voxel.y == 0 && voxel.part == TreePart::Trunk ? 1 : 0;
    // The radius of an oak is 1-2 m: a plus sign at least.
    Require(baseCells >= 5, "an oak trunk is thicker than a single cell");
  }
}

TEST_CASE("TheFormsLookAlike") {
  const Species& oak = SpeciesNamed("species.oak");
  const Species& spruce = SpeciesNamed("species.spruce");
  const Species& palm = SpeciesNamed("species.palm");
  for (std::uint64_t seed = 0; seed < kSeedsPerSpecies; ++seed) {
    const TreeVoxels spruceTree = GrowTree(spruce, seed);
    const double sprucePart = (spruceTree.maxY + 1) / 3.0;
    // A cone: the lower third of the crown is wider than the upper third.
    Require(LeafReach(spruceTree, sprucePart, 2 * sprucePart) >
                LeafReach(spruceTree, 2 * sprucePart, spruceTree.maxY + 1),
            "a spruce narrows towards the top");

    const TreeVoxels palmTree = GrowTree(palm, seed);
    Require(Count(palmTree, TreePart::Branch) == 0 &&
                Count(palmTree, TreePart::Twig) == 0,
            "a palm has no branches");
    Require(LeafReach(palmTree, palmTree.maxY - 3.0, palmTree.maxY + 1) >
                LeafReach(palmTree, 0, palmTree.maxY * 0.5),
            "a palm carries its leaves at the top");

    const TreeVoxels oakTree = GrowTree(oak, seed);
    Require(Count(oakTree, TreePart::Branch) > 0 &&
                Count(oakTree, TreePart::Twig) > 0,
            "an oak has branches and twigs");
    Require(Count(oakTree, TreePart::Root) > 0, "an oak has roots");
    Require(Count(oakTree, TreePart::Leaves) > Count(oakTree, TreePart::Trunk),
            "an oak is mostly leaves");
  }
}

TEST_CASE("EveryPartMapsToTheMatterOfTheSpecies") {
  const Species& oak = SpeciesNamed("species.oak");
  CHECK(PartMatter(oak.matter, TreePart::Trunk) ==
        MatterNamed("oak.trunk").id);
  CHECK(PartMatter(oak.matter, TreePart::Branch) ==
        MatterNamed("oak.branch").id);
  CHECK(PartMatter(oak.matter, TreePart::Twig) ==
        MatterNamed("oak.twig").id);
  CHECK(PartMatter(oak.matter, TreePart::Leaves) ==
        MatterNamed("oak.leaves").id);
  CHECK(PartMatter(oak.matter, TreePart::Root) ==
        MatterNamed("oak.root").id);
  CHECK(IsWood(TreePart::Root));
  CHECK(!IsWood(TreePart::Leaves));
}

TEST_CASE("TreeSizesScaleWithTheSeed") {
  // Over many seeds the heights cover most of the species' range.
  for (const Species& species : Catalog().All()) {
    int lowest = 1000;
    int highest = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
      const int height = GrowTree(species, seed).maxY;
      lowest = std::min(lowest, height);
      highest = std::max(highest, height);
    }
    CHECK_MESSAGE(
        highest - lowest >=
            (species.heightMeters.max - species.heightMeters.min) * 0.6,
        species.key);
  }
}
