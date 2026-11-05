#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "blocks/BlockRegistry.h"
#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/community/VegetationLayer.h"
#include "domains/natural/geology/catalog/GeologyCatalog.h"
#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/RegionGenerator.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "gameplay/GameWorld.h"
#include "gameplay/content/GameContent.h"
#include "matter/MatterCatalog.h"
#include "world/Grid.h"
#include "world/PlanetTerrainSource.h"

#define Require(condition, message)                                                                                    \
	do                                                                                                                 \
	{                                                                                                                  \
		if (!(condition))                                                                                              \
		{                                                                                                              \
			FAIL_CHECK(message);                                                                                       \
		}                                                                                                              \
	} while (0)

namespace
{
using namespace Infinitesia;

constexpr int kWidth = 256;
constexpr int kHeight = 192;
constexpr int kDepth = 256;
constexpr int kAnchorLevel = 112;

const std::filesystem::path kAssets = INFINITESIA_ASSET_DIR;

std::shared_ptr<const MatterCatalog> SharedMatter()
{
	static const auto materials =
	    std::make_shared<const MatterCatalog>(MatterCatalog::LoadFromFile(kAssets / "matter" / "matter.xml"));
	return materials;
}

std::shared_ptr<const BlockRegistry> SharedRegistry()
{
	static const auto registry =
	    std::make_shared<const BlockRegistry>(BlockRegistry::Build(SharedMatter(), kAssets / "blocks" / "shapes.json"));
	return registry;
}

// What the registry says about the cell of a terrain source.
bool IsAir(const TerrainSource &source, GridCell cell)
{
	return !source.Registry().Occupied(source.StateAt(cell));
}

bool IsWater(const TerrainSource &source, GridCell cell)
{
	return source.Registry().Has(source.StateAt(cell).matter, Tag::Water);
}

bool IsGround(const TerrainSource &source, GridCell cell)
{
	return !IsAir(source, cell) && !IsWater(source, cell);
}

bool IsRock(const TerrainSource &source, GridCell cell)
{
	const MatterDef *material = source.Registry().GetMatter(source.StateAt(cell).matter);
	return material != nullptr && material->blockClass == BlockClass::Rock;
}

bool IsMade(const TerrainSource &source, GridCell cell, const char *key)
{
	const MatterDef *material = source.Registry().FindMatter(key);
	return material != nullptr && source.StateAt(cell).matter == material->id;
}

const ObjectKindDef &WorkerKind()
{
	static const std::shared_ptr<const GameContent> content =
	    GameContent::LoadFromDirectory(INFINITESIA_GAMEPLAY_ASSET_DIR);
	return content->GetObjectKind("object.worker");
}

// A coarse planet keeps these tests fast in unoptimized builds.
const PlanetMap &SharedMap()
{
	static const PlanetMap map = [] {
		PlanetParameters parameters;
		parameters.latitudeBands = 90;
		parameters.longitudeSegments = 180;
		return PlanetMap(parameters);
	}();
	return map;
}

const RegionGenerator &SharedGenerator()
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
	const RegionGenerator &generator = SharedGenerator();
	const LocalGrid grid = generator.MakeGrid(anchor);
	auto terrain = std::make_shared<const SettlementTerrain>(generator.GetTerrain(), grid, 0.0, 0.0);
	auto source =
	    std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedRegistry());
	return {terrain, source};
}

// First planet cell centre matching a test, e.g. dry land or deep ocean.
template <typename Predicate> CartesianPoint3D FindCell(Predicate matches)
{
	const PlanetMap &map = SharedMap();
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
	return FindCell(
	    [](const PlanetRegion &cell) { return cell.elevationNormalized > 0.05 && cell.elevationNormalized < 0.3; });
}

CartesianPoint3D OceanAnchor()
{
	return FindCell([](const PlanetRegion &cell) { return cell.elevationNormalized < -0.4; });
}

CartesianPoint3D LakeAnchor()
{
	return FindCell([](const PlanetRegion &cell) { return cell.isLake; });
}

// A flat test world: solid bedrock up to y = 1, air above. Good for fluid tests
// that need a small, known shape.
class FlatTestTerrain final : public TerrainSource
{
public:
	BlockState StateAt(GridCell cell) const override
	{
		if (cell.y <= 1)
		{
			return {SharedRegistry()->FindMatter("bedrock")->id, kShapeFull};
		}
		return {};
	}
	const BlockRegistry &Registry() const override
	{
		return *SharedRegistry();
	}
};

// The top or bottom fluid cell of the first column of a given origin.
std::optional<GridCell> FindFluidCell(const TerrainSource &source, FluidOrigin origin, bool bottom)
{
	for (int x = 0; x < kWidth; x += 2)
	{
		for (int z = 0; z < kDepth; z += 2)
		{
			const FluidColumn fluid = source.FluidAt(x, z);
			if (fluid.origin != origin || fluid.topBlock <= fluid.groundBlock)
			{
				continue;
			}
			return GridCell{x, bottom ? fluid.groundBlock + 1 : fluid.topBlock, z};
		}
	}
	return std::nullopt;
}

std::uint64_t TotalFluidVolume(const VoxelWorld &world)
{
	std::uint64_t volume = 0;
	for (const auto &[coordinate, chunk] : world.Fluids().Chunks())
	{
		static_cast<void>(coordinate);
		for (const FluidCell &cell : chunk.cells)
		{
			volume += cell.amountMb;
		}
	}
	return volume;
}
} // namespace

TEST_CASE("TheOriginColumnSitsAtTheAnchorLevel")
{
	const Settlement settlement = MakeSettlement(LandAnchor());
	Require(settlement.source->SurfaceLevelAt(kWidth / 2, kDepth / 2) == kAnchorLevel,
	        "The middle column is not at the anchor level.");
	Require(!IsAir(*settlement.source, {kWidth / 2, kAnchorLevel, kDepth / 2}),
	        "The ground of the middle column is empty.");
	Require(IsAir(*settlement.source, {kWidth / 2, kAnchorLevel + 1, kDepth / 2}),
	        "There is no air above the ground of the middle column.");
}

TEST_CASE("ColumnsAreGroundWithSoilAndRockBelowAndAirAbove")
{
	for (const CartesianPoint3D anchor : {LandAnchor(), OceanAnchor()})
	{
		const Settlement settlement = MakeSettlement(anchor);
		const PlanetTerrainSource &source = *settlement.source;

		for (int x = 0; x < kWidth; x += 17)
		{
			for (int z = 0; z < kDepth; z += 13)
			{
				const int surface = source.SurfaceLevelAt(x, z);
				Require(surface >= 1 && surface <= kHeight - 2, "Surface level is outside the grid.");
				Require(IsGround(source, {x, surface, z}), "The surface block is not solid ground.");

				for (int y = 0; y < surface; ++y)
				{
					Require(IsGround(source, {x, y, z}), "There is a cavity under the surface.");
				}
				Require(IsRock(source, {x, 0, z}) || surface < 4, "The bottom layer is not rock.");
				Require(IsRock(source, {x, surface - 5, z}) || surface < 6,
				        "The ground is not rock below the topsoil.");

				for (int y = surface + 1; y < kHeight; ++y)
				{
					// Water is no longer a block: the fluid is a separate layer.
					Require(IsAir(source, {x, y, z}), "The column above the ground is not air.");
				}
			}
		}
	}
}

TEST_CASE("TheSeaFillsLowGroundOnlyBelowSeaLevel")
{
	const Settlement ocean = MakeSettlement(OceanAnchor());
	Require(ocean.source->WaterTopLevel() > kAnchorLevel, "Sea level is not above an ocean anchor.");
	const FluidColumn sea = ocean.source->FluidAt(kWidth / 2, kDepth / 2);
	Require(sea.HasFluid(), "The sea column is dry.");
	Require(sea.origin == FluidOrigin::Sea, "The sea column has the wrong origin.");
	Require(sea.topBlock == ocean.source->WaterTopLevel(), "The sea does not stop at the waterline.");
	Require(sea.groundBlock < sea.topBlock, "The sea has no depth.");
	Require(IsMade(*ocean.source, {kWidth / 2, kAnchorLevel, kDepth / 2}, "sand"), "The sea floor is not sand.");

	const Settlement land = MakeSettlement(LandAnchor());
	Require(land.source->WaterTopLevel() < 0, "There is sea level inside a highland world.");
	for (int y = 0; y < kHeight; y += 7)
	{
		Require(!IsWater(*land.source, {kWidth / 2, y, kDepth / 2}), "Water on dry highland.");
	}
}

TEST_CASE("ALakeFillsWaterAboveSeaLevel")
{
	const Settlement settlement = MakeSettlement(LakeAnchor());
	const PlanetTerrainSource &source = *settlement.source;
	bool foundLake = false;
	for (int x = 0; x < kWidth && !foundLake; x += 4)
	{
		for (int z = 0; z < kDepth && !foundLake; z += 4)
		{
			const FluidColumn fluid = source.FluidAt(x, z);
			if (fluid.origin != FluidOrigin::Lake)
			{
				continue;
			}
			foundLake = true;
			Require(fluid.HasFluid(), "A lake column says it is dry.");
			Require(fluid.surfaceMeters > 0.0, "The lake surface is not above sea level.");
			Require(fluid.surfaceMeters > fluid.groundMeters, "The lake surface is not above its ground.");
			Require(fluid.depthMeters > 0.0, "The lake has no depth.");
			Require(fluid.groundBlock < fluid.topBlock, "The lake has no fluid blocks.");
		}
	}
	Require(foundLake, "No lake column: the lake was not filled.");
}

TEST_CASE("TerrainBlocksContainNoFluid")
{
	for (const CartesianPoint3D anchor : {LandAnchor(), OceanAnchor(), LakeAnchor()})
	{
		const Settlement settlement = MakeSettlement(anchor);
		const PlanetTerrainSource &source = *settlement.source;
		for (int x = 0; x < kWidth; x += 13)
		{
			for (int z = 0; z < kDepth; z += 11)
			{
				for (int y = 0; y < kHeight; y += 7)
				{
					const BlockState state = source.StateAt({x, y, z});
					Require(!source.Registry().Has(state.matter, Tag::Fluid),
					        "The block terrain still hands out a fluid.");
				}
			}
		}
	}
}

TEST_CASE("TheSeaIsStoredInTheFluidField")
{
	const Settlement settlement = MakeSettlement(OceanAnchor());
	const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});
	VoxelWorld world(grid, settlement.source);
	const std::optional<GridCell> cell = FindFluidCell(*settlement.source, FluidOrigin::Sea, false);
	REQUIRE(cell.has_value());
	world.EnsureChunkContaining(*cell);

	const FluidCell *fluid = world.GetFluid(*cell);
	REQUIRE(fluid != nullptr);
	const MatterDef *water = settlement.source->Registry().FindMatter("water");
	REQUIRE(water != nullptr);
	CHECK(fluid->matter == water->id);
	CHECK(fluid->origin == FluidOrigin::Sea);
	CHECK(fluid->amountMb > 0);

	// The block at the same cell is air, not water.
	CHECK(!settlement.source->Registry().Has(settlement.source->StateAt(*cell).matter, Tag::Fluid));
}

TEST_CASE("TheSeaIsInfiniteAndALakeIsFinite")
{
	{
		const Settlement settlement = MakeSettlement(OceanAnchor());
		const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});
		VoxelWorld world(grid, settlement.source);
		const std::optional<GridCell> cell = FindFluidCell(*settlement.source, FluidOrigin::Sea, true);
		REQUIRE(cell.has_value());
		world.EnsureChunkContaining(*cell);
		const FluidCell original = *world.GetFluid(*cell);
		REQUIRE(original.origin == FluidOrigin::Sea);

		world.Fluids().Set(*cell, {original.matter, 0, FluidOrigin::Sea});
		world.StepFluids();
		const FluidCell *after = world.GetFluid(*cell);
		REQUIRE(after != nullptr);
		CHECK(after->amountMb == 1000); // the sea fills itself again
	}

	{
		const Settlement settlement = MakeSettlement(LakeAnchor());
		const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});
		VoxelWorld world(grid, settlement.source);
		const std::optional<GridCell> cell = FindFluidCell(*settlement.source, FluidOrigin::Lake, true);
		REQUIRE(cell.has_value());
		world.EnsureChunkContaining(*cell);
		const FluidCell original = *world.GetFluid(*cell);
		REQUIRE(original.origin == FluidOrigin::Lake);
		REQUIRE(original.amountMb > 0);

		const std::uint64_t before = TotalFluidVolume(world);
		world.Fluids().Set(*cell, {original.matter, 0, FluidOrigin::Lake});
		world.StepFluids();
		// A lake has no endless source: draining it cannot add water back.
		CHECK(TotalFluidVolume(world) <= before - original.amountMb);
	}
}

TEST_CASE("WaterFillsADugHole")
{
	const Settlement settlement = MakeSettlement(OceanAnchor());
	const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});
	VoxelWorld world(grid, settlement.source);
	const std::optional<GridCell> wetCell = FindFluidCell(*settlement.source, FluidOrigin::Sea, true);
	REQUIRE(wetCell.has_value());
	const int x = wetCell->x;
	const int z = wetCell->z;
	const GridCell hole{x, wetCell->y - 1, z}; // the solid block the water rests on
	world.EnsureChunkContaining(hole);
	world.EnsureChunkContaining(*wetCell);
	REQUIRE(world.GetBlock(hole) != nullptr);
	REQUIRE(world.Registry().Occupied(world.GetBlock(hole)->state));

	world.SetBlock(hole, {kAirMatter, kShapeFull});
	for (int tick = 0; tick < 8; ++tick)
	{
		world.StepFluids();
	}

	const FluidCell *filled = world.GetFluid(hole);
	REQUIRE(filled != nullptr);
	CHECK(filled->amountMb > 0);
}

TEST_CASE("ABlockPlacedInWaterDisplacesIt")
{
	const Settlement settlement = MakeSettlement(OceanAnchor());
	const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});
	VoxelWorld world(grid, settlement.source);
	const std::optional<GridCell> wetCell = FindFluidCell(*settlement.source, FluidOrigin::Sea, true);
	REQUIRE(wetCell.has_value());
	world.EnsureChunkContaining(*wetCell);
	REQUIRE(world.GetFluid(*wetCell) != nullptr);
	REQUIRE(world.GetFluid(*wetCell)->amountMb > 0);

	const MatterId sand = settlement.source->Registry().FindMatter("sand")->id;
	world.SetBlock(*wetCell, {sand, kShapeFull});

	// The block wins the cell: the fluid is pushed out or, when there is no room
	// (the sea reaches the top of the grid here), simply lost.
	const FluidCell *displaced = world.GetFluid(*wetCell);
	CHECK((displaced == nullptr || displaced->amountMb == 0));
	CHECK(world.GetBlock(*wetCell)->state.matter == sand);
}

TEST_CASE("WaterDoesNotPassThroughSolidBlocks")
{
	const Grid grid(GridConfig{5, 5, 5, 1.0f});
	VoxelWorld world(grid, std::make_shared<FlatTestTerrain>());
	world.EnsureChunkContaining({2, 2, 2});

	const MatterId bedrock = SharedRegistry()->FindMatter("bedrock")->id;
	for (int z = 0; z < 5; ++z)
	{
		for (int y = 2; y < 5; ++y)
		{
			world.SetBlock({2, y, z}, {bedrock, kShapeFull});
		}
	}

	const MatterId water = SharedRegistry()->FindMatter("water")->id;
	world.Fluids().Set({1, 2, 2}, {water, 1000, FluidOrigin::None});
	for (int tick = 0; tick < 10; ++tick)
	{
		world.StepFluids();
	}

	for (int z = 0; z < 5; ++z)
	{
		for (int y = 0; y < 5; ++y)
		{
			// The wall itself stays dry, and nothing appears behind it.
			const FluidCell *wall = world.GetFluid({2, y, z});
			CHECK((wall == nullptr || wall->amountMb == 0));
			const FluidCell *behind = world.GetFluid({3, y, z});
			CHECK((behind == nullptr || behind->amountMb == 0));
		}
	}
}

TEST_CASE("WaterRisesInAUBendLikeDwarfFortress")
{
	// A U-shaped tunnel: two shafts joined at the bottom. The left shaft is a
	// sea source up to level 5; the right is open and must fill up to that level.
	const Grid grid(GridConfig{9, 9, 9, 1.0f});
	VoxelWorld world(grid, std::make_shared<FlatTestTerrain>());
	world.EnsureChunkContaining({2, 2, 2});
	const MatterId bedrock = SharedRegistry()->FindMatter("bedrock")->id;
	const auto wall = [&](GridCell cell) { world.SetBlock(cell, {bedrock, kShapeFull}); };

	for (int x = 1; x <= 7; ++x)
	{
		for (int y = 2; y <= 6; ++y)
		{
			wall({x, y, 3});
			wall({x, y, 5});
		}
	}
	for (int y = 2; y <= 6; ++y)
	{
		wall({1, y, 4});
		wall({7, y, 4});
	}
	for (int x = 3; x <= 5; ++x)
	{
		for (int y = 3; y <= 6; ++y)
		{
			wall({x, y, 4});
		}
	}

	const MatterId water = SharedRegistry()->FindMatter("water")->id;
	for (int y = 2; y <= 5; ++y)
	{
		world.Fluids().Set({2, y, 4}, {water, 1000, FluidOrigin::Sea});
	}

	for (int tick = 0; tick < 200; ++tick)
	{
		world.StepFluids();
	}

	// Pressure pushes the water up the right shaft to the source level, but not
	// above it.
	const FluidCell *top = world.GetFluid({6, 5, 4});
	REQUIRE(top != nullptr);
	CHECK(top->amountMb > 0);
	const FluidCell *above = world.GetFluid({6, 6, 4});
	CHECK((above == nullptr || above->amountMb == 0));
}

TEST_CASE("FluidDoesNotMixDifferentMaterials")
{
	const Grid grid(GridConfig{5, 5, 5, 1.0f});
	VoxelWorld world(grid, std::make_shared<FlatTestTerrain>());
	world.EnsureChunkContaining({2, 2, 2});
	const MatterId water = SharedRegistry()->FindMatter("water")->id;
	const MatterId oil = SharedRegistry()->FindMatter("oil")->id;

	world.Fluids().Set({1, 2, 2}, {water, 500, FluidOrigin::None});
	world.Fluids().Set({2, 2, 2}, {oil, 500, FluidOrigin::None});
	for (int tick = 0; tick < 10; ++tick)
	{
		world.StepFluids();
	}

	const FluidCell *oilCell = world.GetFluid({2, 2, 2});
	REQUIRE(oilCell != nullptr);
	CHECK(oilCell->matter == oil);
	CHECK(oilCell->amountMb > 0);

	const FluidCell *waterCell = world.GetFluid({1, 2, 2});
	CHECK(waterCell != nullptr);
	CHECK(waterCell->matter == water);
}

TEST_CASE("FluidFlowIsDeterministic")
{
	auto makeWorld = [](VoxelWorld &world) {
		world.EnsureChunkContaining({2, 2, 2});
		const MatterId water = SharedRegistry()->FindMatter("water")->id;
		const MatterId oil = SharedRegistry()->FindMatter("oil")->id;
		world.Fluids().Set({1, 2, 2}, {water, 700, FluidOrigin::None});
		world.Fluids().Set({3, 2, 2}, {oil, 300, FluidOrigin::None});
		for (int tick = 0; tick < 20; ++tick)
		{
			world.StepFluids();
		}
	};

	const Grid grid(GridConfig{5, 5, 5, 1.0f});
	VoxelWorld first(grid, std::make_shared<FlatTestTerrain>());
	VoxelWorld second(grid, std::make_shared<FlatTestTerrain>());
	makeWorld(first);
	makeWorld(second);

	for (int z = 0; z < 5; ++z)
	{
		for (int y = 0; y < 5; ++y)
		{
			for (int x = 0; x < 5; ++x)
			{
				const FluidCell *left = first.GetFluid({x, y, z});
				const FluidCell *right = second.GetFluid({x, y, z});
				REQUIRE((left == nullptr) == (right == nullptr));
				if (left != nullptr && right != nullptr)
				{
					REQUIRE(*left == *right);
				}
			}
		}
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
			Require(settlement.source->SurfaceLevelAt(x, z) == expected,
			        "World surface differs from the planet terrain.");
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
				Require(first.source->StateAt({x, y, z}) == second.source->StateAt({x, y, z}),
				        "A block differs between two builds of the same terrain.");
				Require(first.source->StateAt({x, y, z}) == first.source->StateAt({x, y, z}),
				        "A block changes between two queries.");
			}
		}
	}
	Require(IsAir(*first.source, {-1, 0, 0}) && IsAir(*first.source, {0, kHeight, 0}),
	        "Cells outside the grid are not empty.");
}

TEST_CASE("ASourceNeedsARegistryThatKnowsItsRocks")
{
	const Settlement land = MakeSettlement(LandAnchor());
	CHECK_THROWS_AS(PlanetTerrainSource(land.terrain, kWidth, kHeight, kDepth, kAnchorLevel, nullptr),
	                std::invalid_argument);

	// The minimum registry has the surface materials but not the rocks of a
	// geology.
	auto geology = [&] {
		auto catalog = std::make_shared<const GeologyCatalog>(
		    GeologyCatalog::Load(SharedMatter(), kAssets / "geology" / "province_columns.json"));
		return std::make_shared<const GeologyModel>(catalog, SharedMap());
	}();
	CHECK_THROWS_AS(PlanetTerrainSource(land.terrain, kWidth, kHeight, kDepth, kAnchorLevel,
	                                    std::make_shared<const BlockRegistry>(BlockRegistry::BuiltIn()), geology),
	                std::invalid_argument);
}

TEST_CASE("AWorldIsBuiltFromThePlanetTerrain")
{
	const Settlement settlement = MakeSettlement(LandAnchor());
	const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});

	const GridCell center{kWidth / 2, kAnchorLevel, kDepth / 2};
	GameWorld world(grid, {{&WorkerKind(), center}}, settlement.source);
	Require(&world.terrain.Registry() == &settlement.source->Registry(),
	        "The world does not expose the registry of its terrain.");

	world.terrain.RequestChunksAround(center, 2, 1);
	Require(world.terrain.GeneratePendingChunks(1000) > 0, "No chunks were generated.");
	Require(world.terrain.GetLoadedChunkCount() > 0, "No chunks are loaded.");

	for (const auto &[coordinate, chunk] : world.terrain.GetChunks())
	{
		for (const Block &block : chunk.blocks)
		{
			Require(block.state == settlement.source->StateAt(block.cell),
			        "A chunk block differs from the terrain source.");
		}
	}

	const Block *ground = world.terrain.GetBlock(center);
	Require(ground != nullptr && world.terrain.Registry().Occupied(ground->state),
	        "The worker has no ground under it.");
	Require(world.terrain.HasSkyAccess(center), "The ground of the middle column is covered.");
	Require(!world.terrain.HasSkyAccess({center.x, kAnchorLevel - 3, center.z}), "A buried block has sky access.");

	// Any material of the registry can be placed and removed.
	const BlockRegistry &registry = world.terrain.Registry();
	const GridCell target{center.x + 1, kAnchorLevel, center.z};
	world.terrain.SetBlock(target, {kAirMatter, kShapeFull});
	world.terrain.SetBlock(target, {registry.FindMatter("sand")->id, kShapeFull});
	Require(world.terrain.GetBlock(target)->state.matter == registry.FindMatter("sand")->id,
	        "A sand block could not be placed.");
	world.terrain.SetBlock(target, {registry.FindMatter("quartz")->id, kShapeFull});
	Require(world.terrain.GetBlock(target)->state.matter == registry.FindMatter("quartz")->id,
	        "A mineral block could not be placed.");

	// What the registry does not know cannot be placed.
	CHECK_THROWS_AS(world.terrain.SetBlock(target, {4000, kShapeFull}), std::invalid_argument);
	CHECK_THROWS_AS(world.terrain.SetBlock(target, {kAirMatter, 77}), std::invalid_argument);
	CHECK_THROWS_AS(world.terrain.SetBlock({-1, 0, 0}, {kAirMatter, kShapeFull}), std::out_of_range);
}

TEST_CASE("TheOriginalNoiseWorldStillWorks")
{
	const Grid grid(GridConfig{64, 32, 64, 1.0f});
	GameWorld world(grid, {{&WorkerKind(), {32, 31, 32}}}, 7u);
	world.terrain.RequestChunksAround({32, 31, 32}, 1, 1);
	world.terrain.GeneratePendingChunks(100);

	const BlockRegistry &registry = world.terrain.Registry();
	const Block *surface = world.terrain.GetBlock({32, 31, 32});
	Require(surface != nullptr && registry.Occupied(surface->state), "The noise world has no surface.");
	const Block *deep = world.terrain.GetBlock({32, 5, 32});
	Require(deep != nullptr && deep->state.matter == registry.FindMatter("bedrock")->id,
	        "The noise world is not bedrock below the topsoil.");

	GameWorld again(grid, {{&WorkerKind(), {32, 31, 32}}}, 7u);
	again.terrain.RequestChunksAround({32, 31, 32}, 1, 1);
	again.terrain.GeneratePendingChunks(100);
	Require(again.terrain.GetBlock({33, 31, 33})->state == world.terrain.GetBlock({33, 31, 33})->state,
	        "The noise world is not deterministic.");
}

namespace
{
std::shared_ptr<const GeologyModel> SharedGeology()
{
	static const std::shared_ptr<const GeologyModel> model = [] {
		auto catalog = std::make_shared<const GeologyCatalog>(
		    GeologyCatalog::Load(SharedMatter(), kAssets / "geology" / "province_columns.json"));
		return std::make_shared<const GeologyModel>(catalog, SharedMap());
	}();
	return model;
}

std::shared_ptr<PlanetTerrainSource> MakeGeologicalSource(CartesianPoint3D anchor)
{
	const RegionGenerator &generator = SharedGenerator();
	const LocalGrid grid = generator.MakeGrid(anchor);
	auto terrain = std::make_shared<const SettlementTerrain>(generator.GetTerrain(), grid, 0.0, 0.0);
	return std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedRegistry(),
	                                             SharedGeology());
}
} // namespace

TEST_CASE("GeologyChoosesTheRockWithoutChangingWhatIsSolid")
{
	for (const CartesianPoint3D anchor : {LandAnchor(), OceanAnchor()})
	{
		const Settlement plain = MakeSettlement(anchor);
		const auto geological = MakeGeologicalSource(anchor);
		const BlockRegistry &registry = geological->Registry();

		int rockBlocks = 0;
		for (int x = 0; x < kWidth; x += 11)
		{
			for (int z = 0; z < kDepth; z += 7)
			{
				for (int y = 0; y < kHeight; y += 3)
				{
					const GridCell cell{x, y, z};
					const BlockState without = plain.source->StateAt(cell);
					const BlockState with = geological->StateAt(cell);
					Require(registry.Occupied(without) == registry.Occupied(with), "Geology changed what is solid.");
					Require(without.shape == with.shape, "Geology changed a shape.");

					if (IsRock(*plain.source, cell))
					{
						// Bedrock stands in for the rock where there is no geology.
						Require(without.matter == registry.FindMatter("bedrock")->id,
						        "Rock without geology is not bedrock.");
						const MatterDef *info = registry.GetMatter(with.matter);
						Require(info != nullptr && info->blockClass == BlockClass::Rock,
						        "A rock block is not a catalog rock.");
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
		Require(IsAir(*geological, {-1, 5, 5}), "A cell outside the grid is not empty.");
	}
}

TEST_CASE("RocksAreLaidOutInTheLayersOfTheProvincePile")
{
	const auto source = MakeGeologicalSource(LandAnchor());
	const Settlement settlement = MakeSettlement(LandAnchor());
	const SettlementTerrain &terrain = *settlement.terrain;
	const GeologyModel &model = *SharedGeology();

	for (int x = 3; x < kWidth; x += 37)
	{
		for (int z = 5; z < kDepth; z += 41)
		{
			const SurfaceColumn column = terrain.Column(x - kWidth / 2, kDepth / 2 - z);
			const int surface = source->SurfaceLevelAt(x, z);
			// Between the topsoil and the bottom of the window the block is the rock
			// the province's pile gives at its elevation, whichever way it is asked.
			for (int y = 0; y + 5 < surface; y += 9)
			{
				const int elevation = terrain.OriginElevationMeters() + (y - kAnchorLevel);
				const double east = terrain.OriginEastMeters() + (x - kWidth / 2) + 0.5;
				const double north = terrain.OriginNorthMeters() + (kDepth / 2 - z) + 0.5;
				Require(source->StateAt({x, y, z}).matter ==
				            model.BedrockAt(column.province, east, north, elevation, column.pileTopMeters),
				        "A rock block disagrees with the structure of its province.");
			}
		}
	}
}

TEST_CASE("WorldChunksCarryTheStateOfEachBlock")
{
	const auto source = MakeGeologicalSource(LandAnchor());
	const Grid grid(GridConfig{kWidth, kHeight, kDepth, 1.0f});
	const GridCell center{kWidth / 2, kAnchorLevel, kDepth / 2};
	GameWorld world(grid, {{&WorkerKind(), center}}, source);
	Require(&world.terrain.Registry() == &source->Registry(), "The world does not expose the registry.");

	world.terrain.RequestChunksAround(center, 1, 1);
	Require(world.terrain.GeneratePendingChunks(1000) > 0, "No chunks were generated.");
	int rock = 0;
	for (const auto &[coordinate, chunk] : world.terrain.GetChunks())
	{
		for (const Block &block : chunk.blocks)
		{
			Require(block.state == source->StateAt(block.cell), "A chunk block has the wrong state.");
			const MatterDef *material = world.terrain.Registry().GetMatter(block.state.matter);
			rock += material != nullptr && material->blockClass == BlockClass::Rock ? 1 : 0;
		}
	}
	Require(rock > 0, "No chunk block carries a rock.");

	// A dug-out block is air again and no longer remembers its rock.
	const GridCell below{center.x, kAnchorLevel - 8, center.z};
	Require(world.terrain.Registry().Occupied(world.terrain.GetBlock(below)->state),
	        "The block under the ground is empty.");
	world.terrain.SetBlock(below, {kAirMatter, kShapeFull});
	Require(world.terrain.GetBlock(below)->state == BlockState(), "A dug-out block kept its rock.");
}

namespace
{
std::shared_ptr<const SpeciesCatalog> SharedSpecies()
{
	static const auto species = std::make_shared<const SpeciesCatalog>(
	    SpeciesCatalog::Load(kAssets / "biology" / "species.json", *SharedMatter()));
	return species;
}

std::shared_ptr<PlanetTerrainSource> MakeForestedSource(CartesianPoint3D anchor, std::uint64_t seed)
{
	const RegionGenerator &generator = SharedGenerator();
	const LocalGrid grid = generator.MakeGrid(anchor);
	auto terrain = std::make_shared<const SettlementTerrain>(generator.GetTerrain(), grid, 0.0, 0.0);
	return std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedRegistry(),
	                                             nullptr, SharedSpecies(), seed);
}

// Warm and wet enough for trees, on low land.
CartesianPoint3D ForestAnchor()
{
	return FindCell([](const PlanetRegion &cell) {
		return cell.elevationNormalized > 0.02 && cell.elevationNormalized < 0.3 && cell.temperatureCelsius > 5.0 &&
		       cell.temperatureCelsius < 20.0 && cell.precipitationMm > 800.0;
	});
}

bool IsPlant(const TerrainSource &source, GridCell cell)
{
	return source.Registry().Has(source.StateAt(cell).matter, Tag::Plant);
}
} // namespace

TEST_CASE("ForestsGrowOnWetTemperateLand")
{
	const auto source = MakeForestedSource(ForestAnchor(), 11);
	int plants = 0;
	for (int z = 0; z < kDepth; z += 2)
	{
		for (int x = 0; x < kWidth; x += 2)
		{
			const int surface = source->SurfaceLevelAt(x, z);
			for (int y = surface + 1; y < surface + 40 && y < kHeight; ++y)
				plants += IsPlant(*source, {x, y, z}) ? 1 : 0;
		}
	}
	CHECK(plants > 2000);
}

TEST_CASE("TreesStandInTheAirAndRootInSoilNeverInRock")
{
	const auto source = MakeForestedSource(ForestAnchor(), 11);
	const BlockRegistry &registry = source->Registry();
	int roots = 0;
	for (int z = 0; z < kDepth; z += 3)
	{
		for (int x = 0; x < kWidth; x += 3)
		{
			const int surface = source->SurfaceLevelAt(x, z);
			for (int y = 0; y <= surface; ++y)
			{
				const BlockState state = source->StateAt({x, y, z});
				if (!registry.Has(state.matter, Tag::Plant))
					continue;
				Require(y >= surface - 3, "a root goes deeper than the topsoil");
				Require(!IsRock(*source, {x, y, z}), "a root is in rock");
				++roots;
			}
		}
	}
	CHECK(roots > 0);
}

TEST_CASE("TheSettlersArriveInAClearing")
{
	const auto source = MakeForestedSource(ForestAnchor(), 11);
	for (int dz = -5; dz <= 5; ++dz)
	{
		for (int dx = -5; dx <= 5; ++dx)
		{
			const int x = kWidth / 2 + dx;
			const int z = kDepth / 2 + dz;
			const int surface = source->SurfaceLevelAt(x, z);
			for (int up = 1; up <= 2; ++up)
			{
				const BlockState state = source->StateAt({x, surface + up, z});
				Require(!source->Registry().Occupied(state), "a trunk stands in the clearing of the settlers: dx "
				                                                 << dx << " dz " << dz << " up " << up << " material "
				                                                 << source->Registry().GetMatter(state.matter)->key);
			}
		}
	}
}

TEST_CASE("TheForestIsTheSameForTheSameSeedAndDifferentForAnother")
{
	const CartesianPoint3D anchor = ForestAnchor();
	const auto first = MakeForestedSource(anchor, 11);
	const auto second = MakeForestedSource(anchor, 11);
	const auto other = MakeForestedSource(anchor, 12);
	int differing = 0;
	for (int z = 0; z < kDepth; z += 3)
	{
		for (int x = 0; x < kWidth; x += 3)
		{
			const int y = first->SurfaceLevelAt(x, z) + 3;
			Require(first->StateAt({x, y, z}) == second->StateAt({x, y, z}), "one seed grows two forests");
			differing += first->StateAt({x, y, z}) != other->StateAt({x, y, z}) ? 1 : 0;
		}
	}
	CHECK(differing > 0);
}

TEST_CASE("NoTreesGrowInTheSea")
{
	const auto source = MakeForestedSource(OceanAnchor(), 11);
	for (int z = 0; z < kDepth; z += 4)
	{
		for (int x = 0; x < kWidth; x += 4)
		{
			for (int y = 0; y < kHeight; y += 2)
				Require(!IsPlant(*source, {x, y, z}), "a tree grows in the sea");
		}
	}
}

TEST_CASE("TheRegistryMustKnowThePlantsOfTheCatalog")
{
	const RegionGenerator &generator = SharedGenerator();
	auto terrain =
	    std::make_shared<const SettlementTerrain>(generator.GetTerrain(), generator.MakeGrid(LandAnchor()), 0.0, 0.0);
	CHECK_THROWS_AS(PlanetTerrainSource(terrain, kWidth, kHeight, kDepth, kAnchorLevel,
	                                    std::make_shared<const BlockRegistry>(BlockRegistry::BuiltIn()), nullptr,
	                                    SharedSpecies(), 1),
	                std::invalid_argument);
}

namespace
{
// The same coarse planet, grown with the vegetation stage.
const PlanetMap &VegetationMap()
{
	static const PlanetMap map = [] {
		PlanetParameters parameters;
		parameters.latitudeBands = 90;
		parameters.longitudeSegments = 180;
		PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
		AddVegetationStages(pipeline, SharedSpecies());
		return PlanetMap(parameters, {}, pipeline);
	}();
	return map;
}

std::shared_ptr<PlanetTerrainSource> MakeSourceOnVegetationMap(CartesianPoint3D anchor)
{
	static const RegionGenerator generator(VegetationMap());
	auto terrain =
	    std::make_shared<const SettlementTerrain>(generator.GetTerrain(), generator.MakeGrid(anchor), 0.0, 0.0);
	return std::make_shared<PlanetTerrainSource>(terrain, kWidth, kHeight, kDepth, kAnchorLevel, SharedRegistry(),
	                                             nullptr, SharedSpecies(), 11);
}

CartesianPoint3D FindOnVegetationMap(bool forested)
{
	const PlanetMap &map = VegetationMap();
	const auto &layer = map.Layers().Require<VegetationCell>(kVegetationLayer);
	for (int latitude = 10; latitude < map.GetLatitudeBandCount() - 10; ++latitude)
	{
		for (int longitude = 0; longitude < map.GetLongitudeSegmentCount(); ++longitude)
		{
			const PlanetRegion &cell = map.GetRegion(latitude, longitude);
			const float canopy = layer.At(latitude, longitude).canopyCover;
			const bool land = cell.elevationNormalized > 0.05 && cell.elevationNormalized < 0.3;
			if (land && (forested ? canopy > 0.5f : canopy == 0.0f))
				return map.GetPlanet().GetSurfacePoint(latitude, longitude);
		}
	}
	FAIL("The test planet has no matching cell.");
	return {};
}

int CountPlants(const PlanetTerrainSource &source)
{
	int plants = 0;
	for (int z = 0; z < kDepth; z += 2)
	{
		for (int x = 0; x < kWidth; x += 2)
		{
			const int surface = source.SurfaceLevelAt(x, z);
			for (int y = surface + 1; y < surface + 40 && y < kHeight; ++y)
				plants += IsPlant(source, {x, y, z}) ? 1 : 0;
		}
	}
	return plants;
}
} // namespace

TEST_CASE("TheForestFollowsTheVegetationLayerOfThePlanet")
{
	CHECK(CountPlants(*MakeSourceOnVegetationMap(FindOnVegetationMap(true))) > 1000);
	CHECK(CountPlants(*MakeSourceOnVegetationMap(FindOnVegetationMap(false))) == 0);
}
