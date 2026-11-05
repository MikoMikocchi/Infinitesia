#pragma once

#include "core/PlanetaryCoordinates.h"
#include "domains/natural/planetary/PlanetMap.h"

namespace Infinitesia
{
// Planet-grid fields interpolated at an arbitrary point on the sphere.
//
// Elevation and roughness keep their own interpolation (elevation is
// Catmull-Rom so cell borders do not create creases). The climate, soil,
// hydrology and volcanic fields are interpolated bilinearly and carried in
// `surface`, so a biome or a surface kind can be classified at the point
// instead of copied from the nearest 55 km cell.
struct PlanetFieldSample
{
	// The elevation the sample is centred on. On the planet it is the coarse
	// interpolated field; a world-tile overlay replaces it with the tile
	// elevation while `baseElevationMeters` keeps the planet field that drives
	// the detail amplitude and the coast blend.
	double elevationMeters = 0.0;
	double baseElevationMeters = 0.0;
	double roughness = 0.0; // 0..1, from planet-cell slope
	int nearestLatitudeIndex = 0;
	int nearestLongitudeIndex = 0;
	PlanetSurface surface;
};

// Elevation as a pure function of position: the interpolated planet field plus
// zero-mean fractal detail. Because the result depends only on the position,
// neighbouring regions, sites and blocks agree wherever they overlap, and finer
// consumers just ask for a smaller wavelength.
class RegionTerrain
{
public:
	explicit RegionTerrain(const PlanetMap &map) noexcept;

	const PlanetMap &GetPlanetMap() const noexcept
	{
		return m_map;
	}

	// The planet fields at a point of the surface. This is the primary sampling
	// entry point; the direction overload below exists for the globe and for
	// tools that already work in 3D.
	PlanetFieldSample SampleAt(const GeographicCoordinates &position) const noexcept;
	PlanetFieldSample SamplePlanet(CartesianPoint3D direction) const noexcept;

	// Detail octaves run from kLargestDetailWavelengthMeters down to
	// minWavelengthMeters (octaves below it are skipped).
	double ElevationMeters(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept;
	double ElevationMeters(const GeographicCoordinates &position, const PlanetFieldSample &planet,
	                       double minWavelengthMeters) const noexcept;
	double ElevationMeters(CartesianPoint3D direction, double minWavelengthMeters) const noexcept;
	double ElevationMeters(CartesianPoint3D direction, const PlanetFieldSample &planet,
	                       double minWavelengthMeters) const noexcept;

	static constexpr double kLargestDetailWavelengthMeters = 16384.0;

private:
	const PlanetMap &m_map;
	double m_radiusMeters;
	std::uint32_t m_seed;
	// Mean of the southernmost (0) and northernmost (1) planet rows. Every
	// longitude meets at a pole, so the fields have to settle to one value there.
	struct PoleMeans
	{
		double elevation = 0.0;
		double roughness = 0.0;
		PlanetSurface surface;
	};
	PoleMeans m_pole[2];
};
} // namespace Infinitesia
