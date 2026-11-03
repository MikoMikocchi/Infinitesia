#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "geology/model/GeologyModel.h"
#include "planet/PlanetMap.h"
#include "planet/region/RegionGenerator.h"
#include "planet/region/SettlementTerrain.h"
#include "world/Grid.h"
#include "world/PlanetTerrainSource.h"
#include "gameplay/GameWorld.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia;

    constexpr int kWidth = 256;
    constexpr int kHeight = 192;
    constexpr int kDepth = 256;
    constexpr int kAnchorLevel = 112;

    // A coarse planet keeps these tests fast in unoptimized builds.
    const PlanetMap& SharedMap()
    {
        static const PlanetMap map = []
        {
            PlanetParameters parameters;
            parameters.latitudeBands = 90;
            parameters.longitudeSegments = 180;
            return PlanetMap(parameters);
        }();
        return map;
    }

    const RegionGenerator& SharedGenerator()
    {
        static const RegionGenerator generator(SharedMap());
        return generator;
    }

    struct Settlement
    {
        std::shared_ptr<const SettlementTerrain> terrain;
        std::shared_ptr<PlanetTerrainSource> source;
    };

    Settlement MakeSettlement(CartesianPoint3D anchor)
    {
        const RegionGenerator& generator = SharedGenerator();
        const LocalGrid grid = generator.MakeGrid(anchor);
        auto terrain = std::make_shared<const SettlementTerrain>(generator.GetTerrain(), grid, 0.0, 0.0);
        auto source = std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel);
        return { terrain, source };
    }

    // First planet cell centre matching a test, e.g. dry land or deep ocean.
    template <typename Predicate>
    CartesianPoint3D FindCell(Predicate matches)
    {
        const PlanetMap& map = SharedMap();
        for (int latitude = 10; latitude < map.GetLatitudeBandCount() - 10; ++latitude)
        {
            for (int longitude = 0; longitude < map.GetLongitudeSegmentCount(); ++longitude)
            {
                if (matches(map.GetRegion(latitude, longitude)))
                {
                    return map.GetPlanet().GetSurfacePoint(latitude, longitude);
                }
            }
        }
        FAIL("The test planet has no matching cell.");
        return {};
    }

    CartesianPoint3D LandAnchor()
    {
        return FindCell([](const PlanetRegion& cell) { return cell.elevationNormalized > 0.05 && cell.elevationNormalized < 0.3; });
    }

    CartesianPoint3D OceanAnchor()
    {
        return FindCell([](const PlanetRegion& cell) { return cell.elevationNormalized < -0.4; });
    }
}

TEST_CASE("TheOriginColumnSitsAtTheAnchorLevel")
{
    const Settlement settlement = MakeSettlement(LandAnchor());
    Require(settlement.source->SurfaceLevelAt(kWidth / 2, kDepth / 2) == kAnchorLevel,
            "The middle column is not at the anchor level.");
    Require(settlement.source->BlockAt({ kWidth / 2, kAnchorLevel, kDepth / 2 }) != BlockType::Air,
            "The ground of the middle column is empty.");
    Require(settlement.source->BlockAt({ kWidth / 2, kAnchorLevel + 1, kDepth / 2 }) == BlockType::Air,
            "There is no air above the ground of the middle column.");
}

TEST_CASE("ColumnsAreGroundWithSoilAndStoneBelowAndAirAbove")
{
    for (const CartesianPoint3D anchor : { LandAnchor(), OceanAnchor() })
    {
        const Settlement settlement = MakeSettlement(anchor);
        const PlanetTerrainSource& source = *settlement.source;
        const int waterTop = source.WaterTopLevel();

        for (int x = 0; x < kWidth; x += 17)
        {
            for (int z = 0; z < kDepth; z += 13)
            {
                const int surface = source.SurfaceLevelAt(x, z);
                Require(surface >= 1 && surface <= kHeight - 2, "Surface level is outside the grid.");
                const BlockType top = source.BlockAt({ x, surface, z });
                Require(top != BlockType::Air && top != BlockType::Water, "The surface block is not solid ground.");

                for (int y = 0; y < surface; ++y)
                {
                    const BlockType below = source.BlockAt({ x, y, z });
                    Require(below != BlockType::Air && below != BlockType::Water, "There is a cavity under the surface.");
                }
                Require(source.BlockAt({ x, 0, z }) == BlockType::Stone || surface < 4, "The bottom layer is not stone.");
                Require(source.BlockAt({ x, surface - 5, z }) == BlockType::Stone || surface < 6,
                        "The ground is not stone below the topsoil.");

                for (int y = surface + 1; y < kHeight; ++y)
                {
                    const BlockType above = source.BlockAt({ x, y, z });
                    Require(above == (y <= waterTop ? BlockType::Water : BlockType::Air),
                            "The column above the ground is neither air nor sea.");
                }
            }
        }
    }
}

TEST_CASE("TheSeaFillsLowGroundOnlyBelowSeaLevel")
{
    const Settlement ocean = MakeSettlement(OceanAnchor());
    Require(ocean.source->WaterTopLevel() > kAnchorLevel, "Sea level is not above an ocean anchor.");
    Require(ocean.source->BlockAt({ kWidth / 2, ocean.source->WaterTopLevel(), kDepth / 2 }) == BlockType::Water,
            "There is no water above the ocean floor.");
    Require(ocean.source->BlockAt({ kWidth / 2, ocean.source->WaterTopLevel() + 1, kDepth / 2 }) == BlockType::Air,
            "The water does not stop at the waterline.");
    Require(ocean.source->BlockAt({ kWidth / 2, kAnchorLevel, kDepth / 2 }) == BlockType::Sand,
            "The sea floor is not sand.");

    const Settlement land = MakeSettlement(LandAnchor());
    Require(land.source->WaterTopLevel() < 0, "There is sea level inside a highland world.");
    for (int y = 0; y < kHeight; y += 7)
    {
        Require(land.source->BlockAt({ kWidth / 2, y, kDepth / 2 }) != BlockType::Water, "Water on dry highland.");
    }
}

TEST_CASE("SurfaceMatchesThePlanetTerrain")
{
    const Settlement settlement = MakeSettlement(LandAnchor());
    for (int x = 0; x < kWidth; x += 31)
    {
        for (int z = 0; z < kDepth; z += 29)
        {
            const SurfaceColumn column = settlement.terrain->Column(x - kWidth / 2, kDepth / 2 - z);
            const int expected = std::clamp(kAnchorLevel + column.heightBlocks, 1, kHeight - 2);
            Require(settlement.source->SurfaceLevelAt(x, z) == expected, "World surface differs from the planet terrain.");
        }
    }
}

TEST_CASE("TerrainSourcesAreDeterministic")
{
    const Settlement first = MakeSettlement(LandAnchor());
    const Settlement second = MakeSettlement(LandAnchor());
    for (int x = 3; x < kWidth; x += 41)
    {
        for (int y = 90; y < 130; y += 5)
        {
            for (int z = 7; z < kDepth; z += 37)
            {
                Require(first.source->BlockAt({ x, y, z }) == second.source->BlockAt({ x, y, z }),
                        "A block differs between two builds of the same terrain.");
                Require(first.source->BlockAt({ x, y, z }) == first.source->BlockAt({ x, y, z }),
                        "A block changes between two queries.");
            }
        }
    }
    Require(first.source->BlockAt({ -1, 0, 0 }) == BlockType::Air && first.source->BlockAt({ 0, kHeight, 0 }) == BlockType::Air,
            "Cells outside the grid are not empty.");
}

TEST_CASE("AWorldIsBuiltFromThePlanetTerrain")
{
    const Settlement settlement = MakeSettlement(LandAnchor());
    const Grid grid(GridConfig{ kWidth, kHeight, kDepth, 1.0f });

    const GridCell center{ kWidth / 2, kAnchorLevel, kDepth / 2 };
    GameWorld world(grid, { { WorldObjectType::Worker, center } }, settlement.source);

    world.terrain.RequestChunksAround(center, 2, 1);
    Require(world.terrain.GeneratePendingChunks(1000) > 0, "No chunks were generated.");
    Require(world.terrain.GetLoadedChunkCount() > 0, "No chunks are loaded.");

    for (const auto& [coordinate, chunk] : world.terrain.GetChunks())
    {
        for (const Block& block : chunk.blocks)
        {
            Require(block.type == settlement.source->BlockAt(block.cell), "A chunk block differs from the terrain source.");
        }
    }

    const Block* ground = world.terrain.GetBlock(center);
    Require(ground != nullptr && ground->type != BlockType::Air, "The worker has no ground under it.");
    Require(world.terrain.HasSkyAccess(center), "The ground of the middle column is covered.");
    Require(!world.terrain.HasSkyAccess({ center.x, kAnchorLevel - 3, center.z }), "A buried block has sky access.");

    // The new block types can be placed and removed like the old ones.
    const GridCell target{ center.x + 1, kAnchorLevel, center.z };
    world.terrain.SetBlockType(target, BlockType::Air);
    world.terrain.SetBlockType(target, BlockType::Sand);
    world.terrain.SetBlockType(target, BlockType::Water);
    Require(world.terrain.GetBlock(target)->type == BlockType::Water, "A water block could not be placed.");
}

TEST_CASE("TheOriginalNoiseWorldStillWorks")
{
    const Grid grid(GridConfig{ 64, 32, 64, 1.0f });
    GameWorld world(grid, { { WorldObjectType::Worker, { 32, 31, 32 } } }, 7u);
    world.terrain.RequestChunksAround({ 32, 31, 32 }, 1, 1);
    world.terrain.GeneratePendingChunks(100);

    const Block* surface = world.terrain.GetBlock({ 32, 31, 32 });
    Require(surface != nullptr && surface->type != BlockType::Air, "The noise world has no surface.");
    const Block* deep = world.terrain.GetBlock({ 32, 5, 32 });
    Require(deep != nullptr && deep->type == BlockType::Stone, "The noise world is not stone below the topsoil.");

    GameWorld again(grid, { { WorldObjectType::Worker, { 32, 31, 32 } } }, 7u);
    again.terrain.RequestChunksAround({ 32, 31, 32 }, 1, 1);
    again.terrain.GeneratePendingChunks(100);
    Require(again.terrain.GetBlock({ 33, 31, 33 })->type == world.terrain.GetBlock({ 33, 31, 33 })->type,
            "The noise world is not deterministic.");
}

namespace
{
    std::shared_ptr<const GeologyModel> SharedGeology()
    {
        static const std::shared_ptr<const GeologyModel> model = []
        {
            auto catalog = std::make_shared<const GeologyCatalog>(
                GeologyCatalog::LoadFromDirectory(std::filesystem::path(INFINITESIA_GEOLOGY_ASSET_DIR)));
            return std::make_shared<const GeologyModel>(catalog, SharedMap());
        }();
        return model;
    }

    std::shared_ptr<PlanetTerrainSource> MakeGeologicalSource(CartesianPoint3D anchor)
    {
        const RegionGenerator& generator = SharedGenerator();
        const LocalGrid grid = generator.MakeGrid(anchor);
        auto terrain = std::make_shared<const SettlementTerrain>(generator.GetTerrain(), grid, 0.0, 0.0);
        return std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedGeology());
    }
}

TEST_CASE("GeologyAddsRockMaterialsWithoutChangingBlockTypes")
{
    for (const CartesianPoint3D anchor : { LandAnchor(), OceanAnchor() })
    {
        const Settlement plain = MakeSettlement(anchor);
        const auto geological = MakeGeologicalSource(anchor);
        const GeologyCatalog& catalog = *geological->Catalog();
        Require(plain.source->Catalog() == nullptr, "A terrain without geology exposes a catalog.");

        int rockBlocks = 0;
        for (int x = 0; x < kWidth; x += 11)
        {
            for (int z = 0; z < kDepth; z += 7)
            {
                for (int y = 0; y < kHeight; y += 3)
                {
                    const GridCell cell{ x, y, z };
                    const BlockType type = geological->BlockAt(cell);
                    Require(type == plain.source->BlockAt(cell), "Geology changed a block type.");
                    Require(plain.source->MaterialAt(cell) == kUnspecifiedMaterial, "A terrain without geology names a material.");

                    const MaterialId material = geological->MaterialAt(cell);
                    if (type == BlockType::Stone)
                    {
                        const MaterialDef* info = catalog.GetMaterial(material);
                        Require(info != nullptr && info->blockClass == BlockClass::Rock, "A stone block is not a catalog rock.");
                        ++rockBlocks;
                    }
                    else
                    {
                        Require(material == kUnspecifiedMaterial, "A block that is not stone has a rock material.");
                    }
                }
            }
        }
        Require(rockBlocks > 0, "The window has no rock to look at.");
        Require(geological->MaterialAt({ -1, 5, 5 }) == kUnspecifiedMaterial, "A cell outside the grid has a material.");
    }
}

TEST_CASE("RocksAreLaidOutInTheLayersOfTheProvincePile")
{
    const auto source = MakeGeologicalSource(LandAnchor());
    const SettlementTerrain& terrain = *MakeSettlement(LandAnchor()).terrain;
    const GeologyModel& model = *SharedGeology();

    for (int x = 3; x < kWidth; x += 37)
    {
        for (int z = 5; z < kDepth; z += 41)
        {
            const SurfaceColumn column = terrain.Column(x - kWidth / 2, kDepth / 2 - z);
            const int surface = source->SurfaceLevelAt(x, z);
            // Between the topsoil and the bottom of the window the block is the rock the
            // province's pile gives at its elevation, whichever way it is asked.
            for (int y = 0; y + 5 < surface; y += 9)
            {
                const int elevation = terrain.OriginElevationMeters() + (y - kAnchorLevel);
                const double east = terrain.OriginEastMeters() + (x - kWidth / 2) + 0.5;
                const double north = terrain.OriginNorthMeters() + (kDepth / 2 - z) + 0.5;
                Require(source->MaterialAt({ x, y, z }) == model.BedrockAt(column.province, east, north, elevation, column.pileTopMeters),
                        "A rock block disagrees with the structure of its province.");
            }
        }
    }
}

TEST_CASE("WorldChunksCarryTheRockOfEachBlock")
{
    const auto source = MakeGeologicalSource(LandAnchor());
    const Grid grid(GridConfig{ kWidth, kHeight, kDepth, 1.0f });
    const GridCell center{ kWidth / 2, kAnchorLevel, kDepth / 2 };
    GameWorld world(grid, { { WorldObjectType::Worker, center } }, source);
    Require(world.terrain.GetCatalog() == source->Catalog() && world.terrain.GetCatalog() != nullptr, "The world does not expose the catalog.");

    world.terrain.RequestChunksAround(center, 1, 1);
    Require(world.terrain.GeneratePendingChunks(1000) > 0, "No chunks were generated.");
    int rock = 0;
    for (const auto& [coordinate, chunk] : world.terrain.GetChunks())
    {
        for (const Block& block : chunk.blocks)
        {
            Require(block.material == source->MaterialAt(block.cell), "A chunk block has the wrong material.");
            rock += block.material != kUnspecifiedMaterial ? 1 : 0;
        }
    }
    Require(rock > 0, "No chunk block carries a rock.");

    // A dug-out block is air again and no longer remembers its rock.
    const GridCell below{ center.x, kAnchorLevel - 8, center.z };
    Require(world.terrain.GetBlock(below)->material != kUnspecifiedMaterial, "The block under the ground has no rock.");
    world.terrain.SetBlockType(below, BlockType::Air);
    Require(world.terrain.GetBlock(below)->type == BlockType::Air && world.terrain.GetBlock(below)->material == kUnspecifiedMaterial,
            "A dug-out block kept its rock.");
}
