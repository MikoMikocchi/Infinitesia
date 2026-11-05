#pragma once

#include <cstdint>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/FluidOrigin.h"
#include "domains/natural/planetary/region/LocalFrame.h"
#include "domains/natural/planetary/region/LocalGrid.h"
#include "domains/natural/planetary/region/WorldTerrain.h"

namespace Infinitesia
{
// What the surface of a block column is made of.
enum class SurfaceKind : std::uint8_t
{
	Grass,
	Dirt,
	Sand,
	Stone
};

struct SurfaceColumn
{
	// Top solid block, in whole metres above the ground level of the origin
	// column.
	int heightBlocks = 0;
	SurfaceKind kind = SurfaceKind::Grass;
	// The fluid body over the column, if any, and the elevation of its surface
	// in metres above sea level (0 for the sea, the lake's level for a lake,
	// the channel surface for a river). Both are meaningless when `fluid` is
	// None. The planet layer names only the origin; the world layer maps it to a
	// fluid material.
	FluidOrigin fluid = FluidOrigin::None;
	double fluidLevelMeters = 0.0;
	double fluidDepthMeters = 0.0;
	// Unit direction of a river's surface flow, zero for standing water.
	double flowEast = 0.0;
	double flowNorth = 0.0;
	// Geological province of the planet cell under the column, and the elevation
	// (whole metres above sea level) of the planet's coarse surface there: the
	// reference level the province's rock layers are laid out from.
	ProvinceId province = kNoProvince;
	// In a mixed contact zone, part of the column is borrowed from the province
	// on the other side of the contact.
	ProvinceId secondaryProvince = kNoProvince;
	double secondaryWeight = 0.0;
	int pileTopMeters = 0;
	// The climate and soil of the column, interpolated between the planet cells;
	// the temperature is brought to the local elevation with the standard lapse
	// rate.
	double temperatureCelsius = 0.0;
	double precipitationMm = 0.0;
	double drySeasonMonths = 0.0;
	double soilFertility = 0.0;   // 0..1
	double soilDepth = 0.0;       // 0..1
	double elevationMeters = 0.0; // above sea level
	// Where on the planet the column is, to look up planet layers at it.
	GeographicCoordinates position{};
};

// Turns the continuous planet terrain into block columns around an origin point
// of a LocalGrid: the surface height of every 1 m column, what it is made of,
// and where sea level lies. Block (0, 0) is the origin column and defines
// "ground level" (height 0); east and north offsets count in blocks (metres).
class SettlementTerrain
{
public:
	// Detail finer than this is not worth a block (heights are whole metres).
	static constexpr double kMinWavelengthMeters = 2.0;

	SettlementTerrain(const WorldTerrain &terrain, const LocalGrid &grid, double originEastMeters,
	                  double originNorthMeters);
	// The same terrain around a site whose flat frame is already known.
	SettlementTerrain(const WorldTerrain &terrain, const LocalFrame &frame, double geologyOriginEastMeters,
	                  double geologyOriginNorthMeters);

	const LocalGrid &GetGrid() const noexcept
	{
		return m_grid;
	}
	const PlanetMap &GetPlanetMap() const noexcept
	{
		return m_terrain.GetPlanetMap();
	}

	// The origin column's centre, metres east and north of the grid's anchor.
	double OriginEastMeters() const noexcept
	{
		return m_originEastMeters;
	}
	double OriginNorthMeters() const noexcept
	{
		return m_originNorthMeters;
	}

	// Ground elevation of the origin column, whole metres above sea level.
	int OriginElevationMeters() const noexcept
	{
		return m_originElevationMeters;
	}

	// Sea level in blocks above the origin ground level (negative when the origin
	// is above the sea).
	int SeaLevelBlocks() const noexcept
	{
		return -m_originElevationMeters;
	}

	SurfaceColumn Column(int eastBlocks, int northBlocks) const noexcept;

	// What the ground is made of, given the planet cell's biome and the local
	// elevation.
	static SurfaceKind ClassifySurface(Biome biome, double elevationMeters) noexcept;

private:
	WorldTerrain m_terrain;
	LocalGrid m_grid;
	// Flat frame at the grid anchor; the terrain is sampled through it instead
	// of a tangent plane and gnomonic projection.
	LocalFrame m_frame;
	double m_originEastMeters;
	double m_originNorthMeters;
	int m_originElevationMeters = 0;
};
} // namespace Infinitesia
