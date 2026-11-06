#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/LocalGrid.h"

namespace Infinitesia
{
// Refined fields for one theater tile on a (kPointsPerRegionEdge)^2 lattice.
// Row-major, index = y * kPoints + x; x runs east, y runs north. Border points
// are shared with the neighbouring tiles on every side and hold identical
// values.
struct RegionData
{
	static constexpr int kPoints = LocalGrid::kPointsPerRegionEdge;

	RegionId id{};
	GeographicCoordinates center{};
	double cellSizeMeters = 0.0;
	std::vector<float> elevationMeters;
	std::vector<Biome> biome;
	float minElevationMeters = 0.0f;
	float maxElevationMeters = 0.0f;

	static std::size_t Index(int x, int y) noexcept
	{
		return static_cast<std::size_t>(y) * kPoints + static_cast<std::size_t>(x);
	}
	float ElevationAt(int x, int y) const noexcept
	{
		return elevationMeters[Index(x, y)];
	}
	Biome BiomeAt(int x, int y) const noexcept
	{
		return biome[Index(x, y)];
	}
};

// Lattice covering a square of (2 * radius + 1) region widths centred on a
// region of a LocalGrid, laid out like the grid: x grows east, y grows north,
// and the middle point is the region's centre. The lattice is the grid's own,
// so a patch agrees point for point with the RegionData of the regions it
// covers.
struct RegionPatch
{
	RegionId center{};
	int radiusRegions = 0;
	int points = 0; // points per edge
	double cellSizeMeters = 0.0;
	LocalGrid grid{};
	double originEastMeters = 0.0; // grid coordinates of lattice point (0, 0)
	double originNorthMeters = 0.0;
	std::vector<float> elevationMeters;
	std::vector<Biome> biome;
	// Interpolated climate, soil, hydrology and volcanic fields at every
	// point, so the map and the local world can classify a biome and a surface
	// kind at the point instead of copying the nearest planet cell.
	std::vector<PlanetSurface> surface;

	std::size_t Index(int x, int y) const noexcept
	{
		return static_cast<std::size_t>(y) * points + static_cast<std::size_t>(x);
	}
	float ElevationAt(int x, int y) const noexcept
	{
		return elevationMeters[Index(x, y)];
	}

	// Unit direction of lattice position (x, y); fractional positions are fine.
	CartesianPoint3D Direction(double x, double y) const noexcept
	{
		return grid.Direction(originEastMeters + x * cellSizeMeters, originNorthMeters + y * cellSizeMeters);
	}

	// Inverse of Direction.
	void Project(CartesianPoint3D direction, double &x, double &y) const noexcept
	{
		double east = 0.0;
		double north = 0.0;
		grid.Project(direction, east, north);
		x = (east - originEastMeters) / cellSizeMeters;
		y = (north - originNorthMeters) / cellSizeMeters;
	}
};
} // namespace Infinitesia
