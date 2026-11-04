#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

#include "blocks/BlockRegistry.h"
#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/flora/Tree.h"
#include "gameplay/LocalGame.h"
#include "gameplay/content/GameContent.h"
#include "gameplay/work/TreeFelling.h"
#include "matter/MatterCatalog.h"

#define Require(condition, message) \
  do {                              \
    if (!(condition)) {             \
      FAIL_CHECK(message);          \
    }                               \
  } while (0)
#define RequireFatal(condition, message) \
  do {                                   \
    if (!(condition)) {                  \
      FAIL(message);                     \
    }                                    \
  } while (0)

namespace {
using namespace Infinitesia;

constexpr int kSize = 64;
constexpr int kHeight = 48;
constexpr int kGround =
    8;  // the level of the ground blocks the workers stand on

const std::filesystem::path kAssets = INFINITESIA_ASSET_DIR;

std::shared_ptr<const MatterCatalog> SharedMatter() {
  static const auto materials = std::make_shared<const MatterCatalog>(
      MatterCatalog::LoadFromFile(kAssets / "matter" / "matter.json"));
  return materials;
}

std::shared_ptr<const BlockRegistry> SharedRegistry() {
  static const auto registry =
      std::make_shared<const BlockRegistry>(BlockRegistry::Build(
          SharedMatter(), kAssets / "blocks" / "shapes.json"));
  return registry;
}

const SpeciesCatalog& SpeciesOf() {
  static const SpeciesCatalog catalog = SpeciesCatalog::Load(
      kAssets / "biology" / "species.json", *SharedMatter());
  return catalog;
}

struct PlantedTree {
  const Species* species;
  std::uint64_t seed;
  int x;
  int z;
};

// Flat ground (turf over subsoil) with trees grown from the biology on it,
// trunk base one level above the ground. Roots take the place of soil, as in
// the real terrain, and GroundStateAt shows the soil again.
class ForestTerrain final : public TerrainSource {
 public:
  explicit ForestTerrain(std::vector<PlantedTree> trees)
      : m_trees(std::move(trees)) {
    for (const PlantedTree& tree : m_trees)
      m_voxels.push_back(GrowTree(*tree.species, tree.seed));
  }

  BlockState StateAt(GridCell cell) const override {
    for (std::size_t i = 0; i < m_trees.size(); ++i) {
      const PlantedTree& tree = m_trees[i];
      const int dx = cell.x - tree.x;
      const int dy = cell.y - (kGround + 1);
      const int dz = cell.z - tree.z;
      const TreeVoxels& voxels = m_voxels[i];
      if (dx < voxels.minX || dx > voxels.maxX || dy < voxels.minY ||
          dy > voxels.maxY || dz < voxels.minZ || dz > voxels.maxZ)
        continue;
      for (const TreeVoxel& voxel : voxels.cells) {
        if (voxel.x == dx && voxel.y == dy && voxel.z == dz)
          return {PartMatter(tree.species->matter, voxel.part),
                  kShapeFull};
      }
    }
    return GroundStateAt(cell);
  }

  BlockState GroundStateAt(GridCell cell) const override {
    if (cell.y > kGround) return {kAirMatter, kShapeFull};
    return {SharedRegistry()
                ->FindMatter(cell.y == kGround ? "soil.turf" : "soil.subsoil")
                ->id,
            kShapeFull};
  }

  const BlockRegistry& Registry() const override { return *SharedRegistry(); }

 private:
  std::vector<PlantedTree> m_trees;
  std::vector<TreeVoxels> m_voxels;
};

const Grid& TestGrid() {
  static const Grid grid(GridConfig{kSize, kHeight, kSize, 1.0f});
  return grid;
}

const Species& OakSpecies() { return *SpeciesOf().Find("species.oak"); }

// A world with one oak at (x, z), its chunks generated.
struct Orchard {
  explicit Orchard(std::vector<PlantedTree> trees)
      : terrain(std::make_shared<ForestTerrain>(std::move(trees))),
        world(TestGrid(), terrain) {
    world.RequestChunksAround({kSize / 2, kGround, kSize / 2}, 6, 3);
    world.GeneratePendingChunks(100000);
  }

  std::shared_ptr<ForestTerrain> terrain;
  VoxelWorld world;
};

int Count(const VoxelWorld& world, bool (*test)(const VoxelWorld&, GridCell)) {
  int count = 0;
  for (int z = 0; z < kSize; ++z) {
    for (int y = 0; y < kHeight; ++y) {
      for (int x = 0; x < kSize; ++x) count += test(world, {x, y, z}) ? 1 : 0;
    }
  }
  return count;
}

GridCell BaseOf(const Orchard& orchard, int x, int z) {
  (void)orchard;
  return {x, kGround + 1, z};
}
}  // namespace

TEST_CASE("TheWoodOfATreeIsFoundFromAnyOfItsCells") {
  const Orchard orchard({{&OakSpecies(), 7, 30, 30}});
  const int woodCells = Count(orchard.world, IsTreeWood);
  REQUIRE(woodCells > 50);

  const std::vector<GridCell> fromBase =
      FindTreeWood(orchard.world, BaseOf(orchard, 30, 30));
  CHECK(static_cast<int>(fromBase.size()) == woodCells);

  // From the highest cell of wood the same tree is found.
  GridCell highest = fromBase.front();
  for (const GridCell& cell : fromBase) {
    if (cell.y > highest.y) highest = cell;
  }
  CHECK(static_cast<int>(FindTreeWood(orchard.world, highest).size()) ==
        woodCells);

  // Foliage, ground and air are not trees.
  CHECK(FindTreeWood(orchard.world, {30, kGround, 10}).empty());
  CHECK(FindTreeWood(orchard.world, {5, kGround + 5, 5}).empty());
  for (int y = 0; y < kHeight; ++y) {
    if (IsTreeLeaves(orchard.world, {30, y, 30}) ||
        IsTreeLeaves(orchard.world, {31, y, 30})) {
      CHECK(FindTreeWood(orchard.world, {30, y, 30}).empty());
      break;
    }
  }
}

TEST_CASE("FellingATreeLeavesGroundAndAirWhereItStood") {
  Orchard orchard({{&OakSpecies(), 7, 30, 30}});
  VoxelWorld& world = orchard.world;
  REQUIRE(Count(world, IsTreeLeaves) > 100);

  const FelledTree felled = FellTree(world, BaseOf(orchard, 30, 30));
  CHECK(felled.wood.size() > 50);
  CHECK(felled.woodKilograms > 50 * 400.0);
  CHECK(felled.leavesFallen > 100);

  // Nothing of the tree is left; the ground is the ground again, with soil
  // where roots were.
  CHECK(Count(world, IsTreeWood) == 0);
  CHECK(Count(world, IsTreeLeaves) == 0);
  for (const GridCell& cell : felled.wood) {
    const Block* block = world.GetBlock(cell);
    RequireFatal(block != nullptr, "a felled cell is missing");
    Require(block->state == world.GroundStateAt(cell),
            "a felled cell is not what the ground is there");
  }
  const Block* stump = world.GetBlock({30, kGround, 30});
  CHECK(stump->state.matter ==
        SharedRegistry()->FindMatter("soil.turf")->id);
  CHECK(world.Registry().Walkable(stump->state));

  // Felling it again finds nothing.
  CHECK(FellTree(world, BaseOf(orchard, 30, 30)).wood.empty());
}

TEST_CASE("FoliageHeldByAnotherTreeStaysWhenOneFalls") {
  // Two oaks close enough for their crowns to meet but not their wood.
  Orchard orchard({{&OakSpecies(), 7, 28, 30}, {&OakSpecies(), 11, 38, 30}});
  VoxelWorld& world = orchard.world;

  const std::vector<GridCell> first =
      FindTreeWood(world, BaseOf(orchard, 28, 30));
  const std::vector<GridCell> second =
      FindTreeWood(world, BaseOf(orchard, 38, 30));
  REQUIRE(!first.empty());
  REQUIRE(!second.empty());
  REQUIRE(first.size() + second.size() ==
          static_cast<std::size_t>(
              Count(world, IsTreeWood)));  // two separate trees

  const int leavesBefore = Count(world, IsTreeLeaves);
  FellTree(world, BaseOf(orchard, 28, 30));
  CHECK(static_cast<int>(FindTreeWood(world, BaseOf(orchard, 38, 30)).size()) ==
        static_cast<int>(second.size()));
  const int leavesAfter = Count(world, IsTreeLeaves);
  CHECK(leavesAfter < leavesBefore);
  CHECK(leavesAfter > 50);  // the second oak keeps its crown

  FellTree(world, BaseOf(orchard, 38, 30));
  CHECK(Count(world, IsTreeLeaves) == 0);
}

TEST_CASE("AWorkerFellsATreeAndCarriesTheLogsAway") {
  const auto content =
      GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
  // The tree stands four columns east of the worker, who starts in the middle.
  const auto terrain = std::make_shared<ForestTerrain>(
      std::vector<PlantedTree>{{&OakSpecies(), 7, kSize / 2 + 4, kSize / 2}});
  LocalGame game(
      content,
      LocalGameConfig{GridConfig{kSize, kHeight, kSize, 1.0f},
                      WorkerConfig{3.0f, 4.0f, 3.0f, 7000.0f}, 20.0f},
      terrain);

  GameWorld& world = game.GetWorld();
  const GridCell start = game.GetStartingCells().worker;
  REQUIRE(start.y == kGround);
  const auto worker = world.entities.FindObjectAt(start);
  REQUIRE(worker.has_value());
  world.terrain.RequestChunksAround(start, 6, 3);
  world.terrain.GeneratePendingChunks(100000);

  const GridCell trunk{kSize / 2 + 4, kGround + 1, kSize / 2};
  REQUIRE(IsTreeWood(world.terrain, trunk));
  const int woodBefore = Count(world.terrain, IsTreeWood);

  // A cell of leaves is not a tree; a cell of ground is not a tree.
  CHECK(game.GetWorkSystem().AssignChopOrder(
            *worker, {kSize / 2 + 1, kGround, kSize / 2}, world,
            game.GetGrid()) == WorkOrderResult::InvalidTarget);

  REQUIRE(game.GetWorkSystem().AssignChopOrder(*worker, trunk, world,
                                               game.GetGrid()) ==
          WorkOrderResult::Accepted);
  // The same tree cannot be given twice, nor can the worker take a second
  // order.
  CHECK(game.GetWorkSystem().AssignChopOrder(*worker, trunk, world,
                                             game.GetGrid()) ==
        WorkOrderResult::WorkerBusy);

  for (int frame = 0; frame < 20 * 60 && IsTreeWood(world.terrain, trunk);
       ++frame)
    game.AdvanceFrame(0.05f);
  CHECK(Count(world.terrain, IsTreeWood) == 0);
  CHECK(Count(world.terrain, IsTreeLeaves) == 0);
  CHECK(woodBefore > 50);

  // The logs lie on the stump: a pile by the roots' ground, in total as heavy
  // as the wood says.
  int logs = 0;
  std::optional<WorldObjectId> pile;
  for (const WorldObject& object : world.entities.GetObjects()) {
    if (object.item.has_value()) {
      logs += object.item->GetQuantity();
      pile = object.id;
    }
  }
  CHECK(logs >= 1);
  REQUIRE(pile.has_value());
  const WorldObject* lying = world.entities.FindObject(*pile);
  CHECK(lying->cell.y == kGround);
  CHECK(std::abs(lying->cell.x - (kSize / 2 + 4)) <= 3);

  const CommandResult pickUp =
      game.GetCommands().Execute("pick_up_item", worker, *pile, world);
  REQUIRE(pickUp.accepted);
  for (int frame = 0;
       frame < 20 * 60 && world.entities.FindObject(*pile) != nullptr; ++frame)
    game.AdvanceFrame(0.05f);
  CHECK(world.entities.FindObject(*pile) == nullptr);
}

TEST_CASE("ATreeCannotBeDugBlockByBlock") {
  const auto content =
      GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
  const auto terrain = std::make_shared<ForestTerrain>(
      std::vector<PlantedTree>{{&OakSpecies(), 7, kSize / 2 + 4, kSize / 2}});
  LocalGame game(
      content,
      LocalGameConfig{GridConfig{kSize, kHeight, kSize, 1.0f},
                      WorkerConfig{3.0f, 4.0f, 3.0f, 7000.0f}, 20.0f},
      terrain);
  GameWorld& world = game.GetWorld();
  const GridCell start = game.GetStartingCells().worker;
  const auto worker = world.entities.FindObjectAt(start);
  world.terrain.RequestChunksAround(start, 6, 3);
  world.terrain.GeneratePendingChunks(100000);
  // The trunk is on the level above the worker's; a dig order at the worker's
  // level would be refused anyway, but plant blocks are not diggable at all.
  const GridCell trunk{kSize / 2 + 4, kGround + 1, kSize / 2};
  CHECK_FALSE(
      world.terrain.Registry().Diggable(world.terrain.GetBlock(trunk)->state));
  CHECK(game.GetWorkSystem().AssignDigOrder(*worker, trunk, world,
                                            game.GetGrid()) ==
        WorkOrderResult::InvalidTarget);
}

TEST_CASE("AWorkerWithNoGroundToStandOnCannotReachATree") {
  const auto content =
      GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
  // The oak stands on a pillar of ground the worker cannot get to: a trench of
  // one level around it.
  class Island final : public TerrainSource {
   public:
    Island() : m_forest({{&OakSpecies(), 7, kSize / 2 + 8, kSize / 2}}) {}
    BlockState StateAt(GridCell cell) const override {
      if (IsTrench(cell.x, cell.z) && cell.y == kGround)
        return {kAirMatter, kShapeFull};
      return m_forest.StateAt(cell);
    }
    BlockState GroundStateAt(GridCell cell) const override {
      return m_forest.GroundStateAt(cell);
    }
    const BlockRegistry& Registry() const override { return *SharedRegistry(); }

   private:
    static bool IsTrench(int x, int z) {
      const int ring =
          std::max(std::abs(x - (kSize / 2 + 8)), std::abs(z - kSize / 2));
      return ring >= 5 && ring <= 6;
    }
    ForestTerrain m_forest;
  };
  LocalGame game(
      content,
      LocalGameConfig{GridConfig{kSize, kHeight, kSize, 1.0f},
                      WorkerConfig{3.0f, 4.0f, 3.0f, 7000.0f}, 20.0f},
      std::make_shared<Island>());
  GameWorld& world = game.GetWorld();
  const GridCell start = game.GetStartingCells().worker;
  const auto worker = world.entities.FindObjectAt(start);
  world.terrain.RequestChunksAround(start, 6, 3);
  world.terrain.GeneratePendingChunks(100000);
  CHECK(game.GetWorkSystem().AssignChopOrder(
            *worker, {kSize / 2 + 8, kGround + 1, kSize / 2}, world,
            game.GetGrid()) == WorkOrderResult::NoReachableWorkCell);
}
