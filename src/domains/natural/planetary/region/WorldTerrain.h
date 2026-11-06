#pragma once

#include <memory>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/FluidOrigin.h"
#include "domains/natural/planetary/region/RegionTerrain.h"
#include "domains/natural/planetary/world/WorldMap.h"

namespace Infinitesia
{
// Water at one point of the local world, derived from the region tile and the
// continuous terrain. The surface is in metres above sea level; `incision` is
// the depth by which a river channel has been cut into the ground (zero away
// from a channel). `depthMeters` is the standing-water depth for a sea or lake,
// and the channel depth for a river.
struct LocalWater
{
	FluidOrigin origin = FluidOrigin::None;
	double surfaceMeters = 0.0;
	double depthMeters = 0.0;
	// Unit direction of the surface flow; zero where nothing flows.
	double flowEast = 0.0;
	double flowNorth = 0.0;
	// How far the ground has been lowered by a river channel at this point.
	double incisionMeters = 0.0;
	// How far the ground has been raised to form a bank above the river
	// surface. A bank must never sit below the water, or the fluid field will
	// level the river across the surrounding land.
	double bankRaiseMeters = 0.0;

	bool HasWater() const noexcept
	{
		return origin != FluidOrigin::None;
	}
};

// Planet terrain with an optional world-map overlay. Inside the 257 x 257
// window the base elevation, climate, soil and province come from the region
// tile; the fine detail still comes from RegionTerrain, so neighbouring local
// grids agree and the world tile is a real base rather than a second copy of
// the terrain. Rivers are cut into that terrain by WaterAt's incision and the
// water of seas, lakes and rivers is handed to SettlementTerrain.
class WorldTerrain
{
public:
	WorldTerrain(const PlanetMap &map, std::shared_ptr<const WorldMap> world = nullptr) noexcept;

	const RegionTerrain &Planet() const noexcept
	{
		return m_planet;
	}
	const WorldMap *World() const noexcept
	{
		return m_world.get();
	}

	void SetWorld(std::shared_ptr<const WorldMap> world) noexcept
	{
		m_world = std::move(world);
	}
	const PlanetMap &GetPlanetMap() const noexcept
	{
		return m_planet.GetPlanetMap();
	}

	static constexpr double kLargestDetailWavelengthMeters = RegionTerrain::kLargestDetailWavelengthMeters;

	PlanetFieldSample SampleAt(const GeographicCoordinates &position) const noexcept;
	PlanetFieldSample SamplePlanet(CartesianPoint3D direction) const noexcept;

	double ElevationMeters(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept;
	double ElevationMeters(const GeographicCoordinates &position, const PlanetFieldSample &planet,
	                       double minWavelengthMeters) const noexcept;
	double ElevationMeters(CartesianPoint3D direction, double minWavelengthMeters) const noexcept;
	double ElevationMeters(CartesianPoint3D direction, const PlanetFieldSample &planet,
	                       double minWavelengthMeters) const noexcept;

	// Water and river incision at a point. Without a world map this is empty.
	LocalWater WaterAt(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept;

private:
	static PlanetFieldSample ApplyTile(PlanetFieldSample sample, const RegionTile *tile) noexcept;
	// `naturalGroundMeters` is the uncarved ground at `position`; the caller
	// usually has it already, and the bank profile needs it to decide how far
	// to cut or raise.
	LocalWater WaterAt(const GeographicCoordinates &position, double naturalGroundMeters,
	                   double minWavelengthMeters) const noexcept;

	RegionTerrain m_planet;
	std::shared_ptr<const WorldMap> m_world;
};
} // namespace Infinitesia
