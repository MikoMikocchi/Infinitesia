#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "blocks/BlockRegistry.h"
#include "domains/natural/geology/catalog/GeologyCatalog.h"
#include "domains/natural/geology/model/GeologyModel.h"
#include "materials/MaterialCatalog.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/RegionGenerator.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "world/Grid.h"
#include "world/PlanetTerrainSource.h"
#include "gameplay/GameWorld.h"
#include "gameplay/content/GameContent.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

#define Require(condition, message)     do { if (!(condition)) { FAIL_CHECK(message); } } while (0)

namespace
{
    using namespace Infinitesia;

    constexpr int kWidth = 256;
    constexpr int kHeight = 192;
    constexpr int kDepth = 256;
    constexpr int kAnchorLevel = 112;

    const std::filesystem::path kAssets = INFINITESIA_ASSET_DIR;

    std::shared_ptr<const MaterialCatalog> SharedMaterials()
    {
        static const auto materials = std::make_shared<const MaterialCatalog>(
            MaterialCatalog::LoadFromFile(kAssets / "materials" / "materials.json"));
        return materials;
    }

    std::shared_ptr<const BlockRegistry> SharedRegistry()
    {
        static const auto registry = std::make_shared<const BlockRegistry>(
            BlockRegistry::Build(SharedMaterials(), kAssets / "blocks" / "shapes.json"));
        return registry;
    }

    // What the registry says about the cell of a terrain source.
    bool IsAir(const TerrainSource& source, GridCell cell)
    {
        return !source.Registry().Occupied(source.StateAt(cell));
    }

    bool IsWater(const TerrainSource& source, GridCell cell)
    {
        return source.Registry().Has(source.StateAt(cell).material, Tag::Water);
    }

    bool IsGround(const TerrainSource& source, GridCell cell)
    {
        return !IsAir(source, cell) && !IsWater(source, cell);
    }

    bool IsRock(const TerrainSource& source, GridCell cell)
    {
        const MaterialDef* material = source.Registry().GetMaterial(source.StateAt(cell).material);
        return material != nullptr && material->blockClass == BlockClass::Rock;
    }

    bool IsMade(const TerrainSource& source, GridCell cell, const char* key)
    {
        const MaterialDef* material = source.Registry().FindMaterial(key);
        return material != nullptr && source.StateAt(cell).material == material->id;
    }

    const ObjectKindDef& WorkerKind()
    {
        static const std::shared_ptr<const GameContent> content =
            GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
        return content->GetObjectKind("object.worker");
    }

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
        auto source = std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedRegistry());
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
    Require(!IsAir(*settlement.source, { kWidth / 2, kAnchorLevel, kDepth / 2 }),
            "The ground of the middle column is empty.");
    Require(IsAir(*settlement.source, { kWidth / 2, kAnchorLevel + 1, kDepth / 2 }),
            "There is no air above the ground of the middle column.");
}

TEST_CASE("ColumnsAreGroundWithSoilAndRockBelowAndAirAbove")
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
                Require(IsGround(source, { x, surface, z }), "The surface block is not solid ground.");

                for (int y = 0; y < surface; ++y)
                {
                    Require(IsGround(source, { x, y, z }), "There is a cavity under the surface.");
                }
                Require(IsRock(source, { x, 0, z }) || surface < 4, "The bottom layer is not rock.");
                Require(IsRock(source, { x, surface - 5, z }) || surface < 6,
                        "The ground is not rock below the topsoil.");

                for (int y = surface + 1; y < kHeight; ++y)
                {
                    const GridCell cell{ x, y, z };
                    Require(y <= waterTop ? IsMade(source, cell, "fluid.seawater") : IsAir(source, cell),
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
    Require(IsMade(*ocean.source, { kWidth / 2, ocean.source->WaterTopLevel(), kDepth / 2 }, "fluid.seawater"),
            "There is no seawater above the ocean floor.");
    Require(IsAir(*ocean.source, { kWidth / 2, ocean.source->WaterTopLevel() + 1, kDepth / 2 }),
            "The water does not stop at the waterline.");
    Require(IsMade(*ocean.source, { kWidth / 2, kAnchorLevel, kDepth / 2 }, "soil.sand"),
            "The sea floor is not sand.");

    const Settlement land = MakeSettlement(LandAnchor());
    Require(land.source->WaterTopLevel() < 0, "There is sea level inside a highland world.");
    for (int y = 0; y < kHeight; y += 7)
    {
        Require(!IsWater(*land.source, { kWidth / 2, y, kDepth / 2 }), "Water on dry highland.");
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
                Require(first.source->StateAt({ x, y, z }) == second.source->StateAt({ x, y, z }),
                        "A block differs between two builds of the same terrain.");
                Require(first.source->StateAt({ x, y, z }) == first.source->StateAt({ x, y, z }),
                        "A block changes between two queries.");
            }
        }
    }
    Require(IsAir(*first.source, { -1, 0, 0 }) && IsAir(*first.source, { 0, kHeight, 0 }),
            "Cells outside the grid are not empty.");
}

TEST_CASE("ASourceNeedsARegistryThatKnowsItsRocks")
{
    const Settlement land = MakeSettlement(LandAnchor());
    CHECK_THROWS_AS(PlanetTerrainSource(land.terrain, kWidth, kHeight, kDepth, kAnchorLevel, nullptr),
                    std::invalid_argument);

    // The minimum registry has the surface materials but not the rocks of a geology.
    auto geology = [&]
    {
        auto catalog = std::make_shared<const GeologyCatalog>(
            GeologyCatalog::Load(SharedMaterials(), kAssets / "geology" / "province_columns.json"));
        return std::make_shared<const GeologyModel>(catalog, SharedMap());
    }();
    CHECK_THROWS_AS(PlanetTerrainSource(land.terrain, kWidth, kHeight, kDepth, kAnchorLevel,
                                        std::make_shared<const BlockRegistry>(BlockRegistry::BuiltIn()), geology),
                    std::invalid_argument);
}

TEST_CASE("AWorldIsBuiltFromThePlanetTerrain")
{
    const Settlement settlement = MakeSettlement(LandAnchor());
    const Grid grid(GridConfig{ kWidth, kHeight, kDepth, 1.0f });

    const GridCell center{ kWidth / 2, kAnchorLevel, kDepth / 2 };
    GameWorld world(grid, { { &WorkerKind(), center } }, settlement.source);
    Require(&world.terrain.Registry() == &settlement.source->Registry(), "The world does not expose the registry of its terrain.");

    world.terrain.RequestChunksAround(center, 2, 1);
    Require(world.terrain.GeneratePendingChunks(1000) > 0, "No chunks were generated.");
    Require(world.terrain.GetLoadedChunkCount() > 0, "No chunks are loaded.");

    for (const auto& [coordinate, chunk] : world.terrain.GetChunks())
    {
        for (const Block& block : chunk.blocks)
        {
            Require(block.state == settlement.source->StateAt(block.cell), "A chunk block differs from the terrain source.");
        }
    }

    const Block* ground = world.terrain.GetBlock(center);
    Require(ground != nullptr && world.terrain.Registry().Occupied(ground->state), "The worker has no ground under it.");
    Require(world.terrain.HasSkyAccess(center), "The ground of the middle column is covered.");
    Require(!world.terrain.HasSkyAccess({ center.x, kAnchorLevel - 3, center.z }), "A buried block has sky access.");

    // Any material of the registry can be placed and removed.
    const BlockRegistry& registry = world.terrain.Registry();
    const GridCell target{ center.x + 1, kAnchorLevel, center.z };
    world.terrain.SetBlock(target, { kAirMaterial, kShapeFull });
    world.terrain.SetBlock(target, { registry.FindMaterial("soil.sand")->id, kShapeFull });
    world.terrain.SetBlock(target, { registry.FindMaterial("fluid.seawater")->id, kShapeFull });
    Require(world.terrain.GetBlock(target)->state.material == registry.FindMaterial("fluid.seawater")->id,
            "A water block could not be placed.");
    world.terrain.SetBlock(target, { registry.FindMaterial("mineral.quartz")->id, kShapeFull });
    Require(world.terrain.GetBlock(target)->state.material == registry.FindMaterial("mineral.quartz")->id,
            "A mineral block could not be placed.");

    // What the registry does not know cannot be placed.
    CHECK_THROWS_AS(world.terrain.SetBlock(target, { 4000, kShapeFull }), std::invalid_argument);
    CHECK_THROWS_AS(world.terrain.SetBlock(target, { kAirMaterial, 77 }), std::invalid_argument);
    CHECK_THROWS_AS(world.terrain.SetBlock({ -1, 0, 0 }, { kAirMaterial, kShapeFull }), std::out_of_range);
}

TEST_CASE("TheOriginalNoiseWorldStillWorks")
{
    const Grid grid(GridConfig{ 64, 32, 64, 1.0f });
    GameWorld world(grid, { { &WorkerKind(), { 32, 31, 32 } } }, 7u);
    world.terrain.RequestChunksAround({ 32, 31, 32 }, 1, 1);
    world.terrain.GeneratePendingChunks(100);

    const BlockRegistry& registry = world.terrain.Registry();
    const Block* surface = world.terrain.GetBlock({ 32, 31, 32 });
    Require(surface != nullptr && registry.Occupied(surface->state), "The noise world has no surface.");
    const Block* deep = world.terrain.GetBlock({ 32, 5, 32 });
    Require(deep != nullptr && deep->state.material == registry.FindMaterial("rock.bedrock")->id,
            "The noise world is not bedrock below the topsoil.");

    GameWorld again(grid, { { &WorkerKind(), { 32, 31, 32 } } }, 7u);
    again.terrain.RequestChunksAround({ 32, 31, 32 }, 1, 1);
    again.terrain.GeneratePendingChunks(100);
    Require(again.terrain.GetBlock({ 33, 31, 33 })->state == world.terrain.GetBlock({ 33, 31, 33 })->state,
            "The noise world is not deterministic.");
}

namespace
{
    std::shared_ptr<const GeologyModel> SharedGeology()
    {
        static const std::shared_ptr<const GeologyModel> model = []
        {
            auto catalog = std::make_shared<const GeologyCatalog>(
                GeologyCatalog::Load(SharedMaterials(), kAssets / "geology" / "province_columns.json"));
            return std::make_shared<const GeologyModel>(catalog, SharedMap());
        }();
        return model;
    }

    std::shared_ptr<PlanetTerrainSource> MakeGeologicalSource(CartesianPoint3D anchor)
    {
        const RegionGenerator& generator = SharedGenerator();
        const LocalGrid grid = generator.MakeGrid(anchor);
        auto terrain = std::make_shared<const SettlementTerrain>(generator.GetTerrain(), grid, 0.0, 0.0);
        return std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedRegistry(),
                                                     SharedGeology());
    }
}

TEST_CASE("GeologyChoosesTheRockWithoutChangingWhatIsSolid")
{
    for (const CartesianPoint3D anchor : { LandAnchor(), OceanAnchor() })
    {
        const Settlement plain = MakeSettlement(anchor);
        const auto geological = MakeGeologicalSource(anchor);
        const BlockRegistry& registry = geological->Registry();

        int rockBlocks = 0;
        for (int x = 0; x < kWidth; x += 11)
        {
            for (int z = 0; z < kDepth; z += 7)
            {
                for (int y = 0; y < kHeight; y += 3)
                {
                    const GridCell cell{ x, y, z };
                    const BlockState without = plain.source->StateAt(cell);
                    const BlockState with = geological->StateAt(cell);
                    Require(registry.Occupied(without) == registry.Occupied(with), "Geology changed what is solid.");
                    Require(without.shape == with.shape, "Geology changed a shape.");

                    if (IsRock(*plain.source, cell))
                    {
                        // Bedrock stands in for the rock where there is no geology.
                        Require(without.material == registry.FindMaterial("rock.bedrock")->id, "Rock without geology is not bedrock.");
                        const MaterialDef* info = registry.GetMaterial(with.material);
                        Require(info != nullptr && info->blockClass == BlockClass::Rock, "A rock block is not a catalog rock.");
                        ++rockBlocks;
                    }
                    else
                    {
                        Require(with == without, "Geology changed a block that is not rock.");
                    }
                }
            }
        }
        Require(rockBlocks > 0, "The window has no rock to look at.");
        Require(IsAir(*geological, { -1, 5, 5 }), "A cell outside the grid is not empty.");
    }
}

TEST_CASE("RocksAreLaidOutInTheLayersOfTheProvincePile")
{
    const auto source = MakeGeologicalSource(LandAnchor());
    const Settlement settlement = MakeSettlement(LandAnchor());
    const SettlementTerrain& terrain = *settlement.terrain;
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
                Require(source->StateAt({ x, y, z }).material == model.BedrockAt(column.province, east, north, elevation, column.pileTopMeters),
                        "A rock block disagrees with the structure of its province.");
            }
        }
    }
}

TEST_CASE("WorldChunksCarryTheStateOfEachBlock")
{
    const auto source = MakeGeologicalSource(LandAnchor());
    const Grid grid(GridConfig{ kWidth, kHeight, kDepth, 1.0f });
    const GridCell center{ kWidth / 2, kAnchorLevel, kDepth / 2 };
    GameWorld world(grid, { { &WorkerKind(), center } }, source);
    Require(&world.terrain.Registry() == &source->Registry(), "The world does not expose the registry.");

    world.terrain.RequestChunksAround(center, 1, 1);
    Require(world.terrain.GeneratePendingChunks(1000) > 0, "No chunks were generated.");
    int rock = 0;
    for (const auto& [coordinate, chunk] : world.terrain.GetChunks())
    {
        for (const Block& block : chunk.blocks)
        {
            Require(block.state == source->StateAt(block.cell), "A chunk block has the wrong state.");
            const MaterialDef* material = world.terrain.Registry().GetMaterial(block.state.material);
            rock += material != nullptr && material->blockClass == BlockClass::Rock ? 1 : 0;
        }
    }
    Require(rock > 0, "No chunk block carries a rock.");

    // A dug-out block is air again and no longer remembers its rock.
    const GridCell below{ center.x, kAnchorLevel - 8, center.z };
    Require(world.terrain.Registry().Occupied(world.terrain.GetBlock(below)->state), "The block under the ground is empty.");
    world.terrain.SetBlock(below, { kAirMaterial, kShapeFull });
    Require(world.terrain.GetBlock(below)->state == BlockState(), "A dug-out block kept its rock.");
}
