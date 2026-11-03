#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gameplay/LocalGame.h"
#include "gameplay/StartingCells.h"
#include "gameplay/content/GameContent.h"
#include "world/TerrainSource.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace
{
    using namespace Infinitesia;

    constexpr int kSize = 64;
    constexpr int kHeight = 24;
    constexpr int kGround = 8;

    // Terrain made of a height function: solid up to and including height(x, z), air above.
    class HeightTerrain final : public TerrainSource
    {
    public:
        explicit HeightTerrain(std::function<int(int, int)> height) : m_height(std::move(height)) {}

        BlockType BlockAt(GridCell cell) const override
        {
            return cell.y <= m_height(cell.x, cell.z) ? BlockType::Stone : BlockType::Air;
        }

    private:
        std::function<int(int, int)> m_height;
    };

    const Grid& TestGrid()
    {
        static const Grid grid(GridConfig{ kSize, kHeight, kSize, 1.0f });
        return grid;
    }

    int Ring(StartingCells cells)
    {
        const int dx = cells.tree->x - cells.worker.x;
        const int dz = cells.tree->z - cells.worker.z;
        return std::max(std::abs(dx), std::abs(dz));
    }
}

TEST_CASE("TheWorkerStartsOnTheGroundOfTheMiddleColumn")
{
    const HeightTerrain terrain([](int, int) { return kGround; });
    const StartingCells cells = FindStartingCells(TestGrid(), terrain);
    CHECK(cells.worker.x == kSize / 2);
    CHECK(cells.worker.z == kSize / 2);
    CHECK(cells.worker.y == kGround);
}

TEST_CASE("WaterIsNotGround")
{
    const class Flooded final : public TerrainSource
    {
    public:
        BlockType BlockAt(GridCell cell) const override
        {
            if (cell.y <= kGround) { return BlockType::Sand; }
            return cell.y <= kGround + 3 ? BlockType::Water : BlockType::Air;
        }
    } terrain;
    CHECK(SurfaceLevelOf(TestGrid(), terrain, 10, 10) == kGround);
    CHECK(SurfaceLevelOf(TestGrid(), terrain, -1, 10) == -1);
}

TEST_CASE("TheTreeIsTheNearestReachableCellOnTheWorkersLevel")
{
    const HeightTerrain terrain([](int, int) { return kGround; });
    const StartingCells cells = FindStartingCells(TestGrid(), terrain);
    REQUIRE(cells.tree.has_value());
    CHECK(Ring(cells) == 3);
    CHECK(cells.tree->y == kGround);
}

TEST_CASE("TheTreeNeverStandsOnHigherOrLowerGround")
{
    // A hill rises east of the worker and a pit sinks west of him; the tree must stand on neither.
    const HeightTerrain terrain([](int x, int z)
    {
        const int dx = x - kSize / 2;
        const int dz = z - kSize / 2;
        if (dx >= 3 && dx <= 5) { return kGround + 2; }
        if (dx <= -3 && dx >= -5) { return kGround - 2; }
        (void)dz;
        return kGround;
    });
    const StartingCells cells = FindStartingCells(TestGrid(), terrain);
    REQUIRE(cells.tree.has_value());
    CHECK(SurfaceLevelOf(TestGrid(), terrain, cells.tree->x, cells.tree->z) == kGround);
}

TEST_CASE("ATreeBehindAPitIsNotReachable")
{
    // A trench one block deeper than the worker's level surrounds a plateau of radius 2:
    // every cell at the worker's level further out lies behind the trench.
    const HeightTerrain terrain([](int x, int z)
    {
        const int ring = std::max(std::abs(x - kSize / 2), std::abs(z - kSize / 2));
        return ring == 3 ? kGround - 1 : kGround;
    });
    const StartingCells cells = FindStartingCells(TestGrid(), terrain);
    CHECK_FALSE(cells.tree.has_value());
}

TEST_CASE("NoTreeIsPlacedWhenNoCellIsReachable")
{
    const HeightTerrain terrain([](int x, int z)
    {
        return x == kSize / 2 && z == kSize / 2 ? kGround : kGround - 1;
    });
    const StartingCells cells = FindStartingCells(TestGrid(), terrain);
    CHECK(cells.worker.y == kGround);
    CHECK_FALSE(cells.tree.has_value());
}

TEST_CASE("AWorkerCanChopTheStartingTreeAndPickUpTheLogs")
{
    const auto content = GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
    const auto terrain = std::make_shared<HeightTerrain>([](int, int) { return kGround; });
    LocalGame game(content, LocalGameConfig{ GridConfig{ kSize, kHeight, kSize, 1.0f }, WorkerConfig{ 3.0f, 4.0f, 3.0f, 3 }, 20.0f },
                   terrain);

    GameWorld& world = game.GetWorld();
    const StartingCells& start = game.GetStartingCells();
    REQUIRE(start.tree.has_value());
    const auto worker = world.entities.FindObjectAt(start.worker);
    const auto tree = world.entities.FindObjectAt(*start.tree);
    REQUIRE(worker.has_value());
    REQUIRE(tree.has_value());

    world.terrain.RequestChunksAround(start.worker, 3, 1);
    world.terrain.GeneratePendingChunks(1000);

    const CommandResult chop = game.GetCommands().Execute("chop_tree", worker, *tree, world);
    REQUIRE(chop.accepted);
    for (int frame = 0; frame < 20 * 60 && world.entities.FindObject(*tree) != nullptr; ++frame)
    {
        game.AdvanceFrame(0.05f);
    }
    CHECK(game.GetSimulation().GetTickCount() > 0);
    CHECK(world.entities.FindObject(*tree) == nullptr);

    // The tree has become an item lying where it stood; pick it up.
    const auto logs = world.entities.FindObjectAt(*start.tree);
    REQUIRE(logs.has_value());
    const CommandResult pickUp = game.GetCommands().Execute("pick_up_item", worker, *logs, world);
    REQUIRE(pickUp.accepted);
    for (int frame = 0; frame < 20 * 60 && world.entities.FindObject(*logs) != nullptr; ++frame)
    {
        game.AdvanceFrame(0.05f);
    }
    CHECK(world.entities.FindObject(*logs) == nullptr);
}
