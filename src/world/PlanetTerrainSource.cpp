#include "world/PlanetTerrainSource.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
namespace
{
// Thickness of the topsoil under the surface block.
constexpr int kTopsoilBlocks = 3;

PlanetTerrainSource::Layer SurfaceLayer(SurfaceKind kind)
{
	switch (kind)
	{
	case SurfaceKind::Grass:
		return PlanetTerrainSource::Layer::Turf;
	case SurfaceKind::Dirt:
		return PlanetTerrainSource::Layer::Subsoil;
	case SurfaceKind::Sand:
		return PlanetTerrainSource::Layer::Sand;
	case SurfaceKind::Stone:
		return PlanetTerrainSource::Layer::Rock;
	}
	return PlanetTerrainSource::Layer::Rock;
}

PlanetTerrainSource::Layer TopsoilLayer(SurfaceKind kind)
{
	switch (kind)
	{
	case SurfaceKind::Grass:
	case SurfaceKind::Dirt:
		return PlanetTerrainSource::Layer::Subsoil;
	case SurfaceKind::Sand:
		return PlanetTerrainSource::Layer::Sand;
	case SurfaceKind::Stone:
		return PlanetTerrainSource::Layer::Rock;
	}
	return PlanetTerrainSource::Layer::Rock;
}
} // namespace

PlanetTerrainSource::PlanetTerrainSource(std::shared_ptr<const SettlementTerrain> terrain, int gridWidth,
                                         int gridHeight, int gridDepth, int anchorLevel,
                                         std::shared_ptr<const BlockRegistry> registry,
                                         std::shared_ptr<const GeologyModel> geology,
                                         std::shared_ptr<const SpeciesCatalog> flora, std::uint64_t floraSeed)
    : m_terrain(std::move(terrain)), m_registry(std::move(registry)), m_geology(std::move(geology)), m_width(gridWidth),
      m_height(gridHeight), m_depth(gridDepth), m_anchorLevel(anchorLevel)
{
	if (m_terrain == nullptr || m_registry == nullptr || gridWidth <= 0 || gridHeight < 4 || gridDepth <= 0 ||
	    anchorLevel < 0 || anchorLevel >= gridHeight)
	{
		throw std::invalid_argument("Planet terrain source needs a terrain, a registry and a valid grid.");
	}
	m_matter = SurfaceMatter::Resolve(*m_registry);
	if (m_geology != nullptr)
	{
		for (int type = 0; type < kProvinceTypeCount; ++type)
		{
			for (const ColumnLayer &layer : m_geology->Catalog().ColumnFor(static_cast<ProvinceType>(type)).bottomUp)
			{
				if (m_registry->GetMatter(layer.rock) == nullptr)
					throw std::invalid_argument("The registry does not know a rock of the geology.");
			}
		}
	}
	if (flora != nullptr)
	{
		for (const Species &species : flora->All())
		{
			for (const TreePart part :
			     {TreePart::Trunk, TreePart::Branch, TreePart::Twig, TreePart::Leaves, TreePart::Root})
			{
				if (m_registry->GetMatter(PartMatter(species.matter, part)) == nullptr)
					throw std::invalid_argument("The registry does not know a material of " + species.key + ".");
			}
		}
		m_ground = std::make_unique<Ground>(*this);
		m_flora = std::make_unique<FloraModel>(std::move(flora), floraSeed, gridWidth, gridDepth, *m_ground);
	}
}

const GeologyModel::ColumnProfile &PlanetTerrainSource::ProfileAt(int x, int z) const
{
	const std::size_t index =
	    static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
	const auto found = m_profiles.find(index);
	if (found != m_profiles.end())
	{
		return found->second;
	}
	const Column &column = ColumnAt(x, z);
	// The centre of the 1 m column, in metres east and north of the grid's
	// anchor.
	const double east = m_terrain->OriginEastMeters() + (x - m_width / 2) + 0.5;
	const double north = m_terrain->OriginNorthMeters() + (m_depth / 2 - z) + 0.5;
	return m_profiles.emplace(index, m_geology->ProfileAt(column.province, east, north, column.pileTopMeters))
	    .first->second;
}

const PlanetTerrainSource::Column &PlanetTerrainSource::ColumnAt(int x, int z) const
{
	const std::size_t index =
	    static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x);
	const auto found = m_columns.find(index);
	if (found != m_columns.end())
	{
		return found->second;
	}
	const SurfaceColumn column = m_terrain->Column(x - m_width / 2, m_depth / 2 - z);
	Column &cached = m_columns[index];
	cached.surface = std::clamp(m_anchorLevel + column.heightBlocks, 1, m_height - 2);
	cached.kind = column.kind;
	cached.province = column.province;
	cached.pileTopMeters = column.pileTopMeters;
	cached.fluid = column.fluid;
	cached.fluidLevelMeters = column.fluidLevelMeters;
	cached.fluidTopBlocks = column.fluid != FluidOrigin::None ? FluidTopBlocksFor(column.fluidLevelMeters) : -1;
	cached.direction = column.direction;
	Conditions &climate = cached.climate;
	climate.temperatureCelsius = column.temperatureCelsius;
	climate.precipitationMm = column.precipitationMm;
	climate.elevationMeters = column.elevationMeters;
	climate.drySeasonMonths = column.drySeasonMonths;
	climate.soilFertility = column.soilFertility;
	climate.soilDepth = column.soilDepth;
	return cached;
}

GroundColumn PlanetTerrainSource::Ground::ColumnAt(int x, int z) const
{
	const Column &column = m_owner.ColumnAt(x, z);
	GroundColumn ground;
	ground.surfaceLevel = column.surface;
	const bool soil = column.kind == SurfaceKind::Grass || column.kind == SurfaceKind::Dirt;
	const bool clearing = std::abs(x - m_owner.m_width / 2) < kStartClearingMeters &&
	                      std::abs(z - m_owner.m_depth / 2) < kStartClearingMeters;
	ground.growable = soil && column.surface > column.fluidTopBlocks && !clearing;
	ground.conditions = column.climate;
	ground.vegetation = SampleVegetation(m_owner.m_terrain->GetPlanetMap(), column.direction);
	// The steepest rise over two metres in the four directions.
	int steepest = 0;
	for (const auto [dx, dz] : {std::pair{2, 0}, std::pair{-2, 0}, std::pair{0, 2}, std::pair{0, -2}})
		steepest = std::max(steepest, std::abs(m_owner.SurfaceLevelAt(x + dx, z + dz) - column.surface));
	ground.conditions.slope = steepest / 2.0;
	return ground;
}

int PlanetTerrainSource::SurfaceLevelAt(int x, int z) const
{
	x = std::clamp(x, 0, m_width - 1);
	z = std::clamp(z, 0, m_depth - 1);
	return ColumnAt(x, z).surface;
}

int PlanetTerrainSource::WaterTopLevel() const
{
	// The sea surface is 0 m.
	return FluidTopBlocksFor(0.0);
}

int PlanetTerrainSource::FluidTopBlocksFor(double fluidLevelMeters) const
{
	// Block y has its bottom at OriginElevation + (y - anchor); it holds fluid
	// when that lies below the surface, so the top fluid block is one below the
	// first block whose bottom reaches the surface.
	const int top =
	    m_anchorLevel + static_cast<int>(std::ceil(fluidLevelMeters)) - m_terrain->OriginElevationMeters() - 1;
	return std::clamp(top, -1, m_height - 1);
}

FluidColumn PlanetTerrainSource::FluidAt(int x, int z) const
{
	if (x < 0 || x >= m_width || z < 0 || z >= m_depth)
	{
		return {};
	}
	const Column &column = ColumnAt(x, z);
	FluidColumn fluid;
	// The planet makes water bodies for now; a volcanic or subsurface origin
	// will pick magma or oil here when those sources are generated. The block
	// grid and FluidColumn do not care which material it is.
	fluid.matter = column.fluid != FluidOrigin::None ? m_matter.water : kAirMatter;
	fluid.origin = column.fluid;
	// The top of the topmost ground block.
	fluid.groundMeters = static_cast<double>(m_terrain->OriginElevationMeters() + column.surface + 1 - m_anchorLevel);
	fluid.surfaceMeters = fluid.HasFluid() ? column.fluidLevelMeters : fluid.groundMeters;
	fluid.depthMeters = fluid.HasFluid() ? std::max(0.0, fluid.surfaceMeters - fluid.groundMeters) : 0.0;
	return fluid;
}

BlockState PlanetTerrainSource::StateAt(GridCell cell) const
{
	return StateOf(cell, true);
}

BlockState PlanetTerrainSource::GroundStateAt(GridCell cell) const
{
	return StateOf(cell, false);
}

BlockState PlanetTerrainSource::StateOf(GridCell cell, bool withPlants) const
{
	if (cell.x < 0 || cell.x >= m_width || cell.y < 0 || cell.y >= m_height || cell.z < 0 || cell.z >= m_depth)
	{
		return {};
	}

	const Column &column = ColumnAt(cell.x, cell.z);
	const FloraCell *plant = withPlants ? FloraAt(cell) : nullptr;
	if (cell.y > column.surface)
	{
		if (cell.y <= column.fluidTopBlocks)
		{
			// Every fluid origin maps to the one surface fluid for now; a later
			// origin table can send volcanic pockets to magma and subsurface ones to
			// oil without touching the grid.
			return {m_matter.water, kShapeFull};
		}
		return {plant != nullptr ? plant->matter : kAirMatter, kShapeFull};
	}
	const Layer layer = LayerAt(column.kind, column.surface - cell.y);
	// Roots grow into the soil; rock stops them.
	if (plant != nullptr && plant->part == TreePart::Root && layer != Layer::Rock)
		return {plant->matter, kShapeFull};
	switch (layer)
	{
	case Layer::Turf:
		return {m_matter.turf, kShapeFull};
	case Layer::Subsoil:
		return {m_matter.subsoil, kShapeFull};
	case Layer::Sand:
		return {m_matter.sand, kShapeFull};
	case Layer::Rock:
		break;
	}
	if (m_geology == nullptr)
	{
		return {m_matter.bedrock, kShapeFull};
	}
	// A level is a metre of height: the ground of the origin column is at
	// anchorLevel and OriginElevationMeters above sea level.
	const int elevation = m_terrain->OriginElevationMeters() + (cell.y - m_anchorLevel);
	return {GeologyModel::RockAt(ProfileAt(cell.x, cell.z), elevation), kShapeFull};
}

const FloraCell *PlanetTerrainSource::FloraAt(GridCell cell) const
{
	if (m_flora == nullptr)
		return nullptr;
	const std::vector<FloraCell> &cells = m_flora->Column(cell.x, cell.z);
	const auto found = std::lower_bound(cells.begin(), cells.end(), cell.y,
	                                    [](const FloraCell &flora, int level) { return flora.level < level; });
	return found != cells.end() && found->level == cell.y ? &*found : nullptr;
}

PlanetTerrainSource::Layer PlanetTerrainSource::LayerAt(SurfaceKind kind, int depthBelowSurface)
{
	if (depthBelowSurface <= 0)
	{
		return SurfaceLayer(kind);
	}
	return depthBelowSurface <= kTopsoilBlocks ? TopsoilLayer(kind) : Layer::Rock;
}
} // namespace Infinitesia
