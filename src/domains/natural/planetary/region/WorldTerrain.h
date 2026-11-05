#pragma once

#include <memory>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/RegionTerrain.h"
#include "domains/natural/planetary/world/WorldMap.h"

namespace Infinitesia
{
// Planet terrain with an optional world-map overlay. Inside the 257 x 257
// window the base elevation, climate, soil and province come from the region
// tile; the fine detail still comes from RegionTerrain, so neighbouring local
// grids agree and the world tile is a real base rather than a second copy of
// the terrain.
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

private:
	static PlanetFieldSample ApplyTile(PlanetFieldSample sample, const RegionTile *tile) noexcept;

	RegionTerrain m_planet;
	std::shared_ptr<const WorldMap> m_world;
};
} // namespace Infinitesia
