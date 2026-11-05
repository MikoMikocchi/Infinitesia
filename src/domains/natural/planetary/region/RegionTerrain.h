#pragma once

#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/PlanetMap.h"

namespace Infinitesia
{
// Planet-grid fields interpolated at an arbitrary direction on the sphere.
struct PlanetFieldSample
{
	double elevationMeters = 0.0; // coarse, bilinear over 55 km cells
	double roughness = 0.0;       // 0..1, from planet-cell slope
	int nearestLatitudeIndex = 0;
	int nearestLongitudeIndex = 0;
};

// Elevation as a pure function of direction: the interpolated planet field
// plus zero-mean fractal detail. Because the result depends only on the
// direction, neighbouring regions, sites and blocks agree wherever they
// overlap, and finer consumers just ask for a smaller wavelength.
class RegionTerrain
{
public:
	explicit RegionTerrain(const PlanetMap &map) noexcept;

	const PlanetMap &GetPlanetMap() const noexcept
	{
		return m_map;
	}

	PlanetFieldSample SamplePlanet(CartesianPoint3D direction) const noexcept;

	// Detail octaves run from kLargestDetailWavelengthMeters down to
	// minWavelengthMeters (octaves below it are skipped).
	double ElevationMeters(CartesianPoint3D direction, double minWavelengthMeters) const noexcept;
	double ElevationMeters(CartesianPoint3D direction, const PlanetFieldSample &planet,
	                       double minWavelengthMeters) const noexcept;

	static constexpr double kLargestDetailWavelengthMeters = 16384.0;

private:
	const PlanetMap &m_map;
	double m_radiusMeters;
	std::uint32_t m_seed;
	// Mean of the southernmost (0) and northernmost (1) planet rows. Every
	// longitude meets at a pole, so the field has to settle to one value there.
	double m_poleElevation[2]{};
	double m_poleRoughness[2]{};
};
} // namespace Infinitesia
