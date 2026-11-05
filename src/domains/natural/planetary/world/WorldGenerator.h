#pragma once

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/RegionTerrain.h"
#include "domains/natural/planetary/world/WorldMap.h"

namespace Infinitesia
{
// Detail added at world-tile centres. The world tile is 768 m, so the shortest
// detail wavelength is twice that (Nyquist); the local region adds the finer
// octaves later.
inline constexpr double kWorldDetailWavelengthMeters = 2.0 * kRegionTileSizeMeters;

// Builds the 257 x 257 world map from a finished planet. The map must outlive
// the generator.
class WorldGenerator
{
public:
	explicit WorldGenerator(const PlanetMap &map) noexcept;

	// `anchorDirection` is a unit direction on the sphere. The world is centred
	// on that point; the anchor is region tile (0, 0).
	WorldMap Generate(CartesianPoint3D anchorDirection) const;
	WorldMap Generate(const GeographicCoordinates &anchor) const;

	const RegionTerrain &Terrain() const noexcept
	{
		return m_terrain;
	}

private:
	double m_radiusMeters;
	RegionTerrain m_terrain;
};
} // namespace Infinitesia
