#include "domains/natural/planetary/region/WorldTerrain.h"

#include <utility>

#include "core/PlanetaryCoordinates.h"

namespace Infinitesia
{
WorldTerrain::WorldTerrain(const PlanetMap &map, std::shared_ptr<const WorldMap> world) noexcept
    : m_planet(map), m_world(std::move(world))
{
}

PlanetFieldSample WorldTerrain::ApplyTile(PlanetFieldSample sample, const RegionTile *tile) noexcept
{
	if (tile == nullptr)
	{
		return sample;
	}

	// The detail amplitude and the coast blend stay tied to the planet field:
	// the tile only replaces the surface the detail is added to.
	sample.baseElevationMeters = sample.elevationMeters;
	sample.elevationMeters = tile->elevationMeters;
	sample.surface.temperatureCelsius = tile->temperatureCelsius;
	sample.surface.precipitationMm = tile->precipitationMm;
	sample.surface.drySeasonMonths = tile->drySeasonMonths;
	sample.surface.soilFertility = tile->soilFertility;
	sample.surface.province = tile->province;
	sample.surface.secondaryProvince = kNoProvince;
	sample.surface.secondaryWeight = 0.0;
	return sample;
}

PlanetFieldSample WorldTerrain::SampleAt(const GeographicCoordinates &position) const noexcept
{
	PlanetFieldSample sample = m_planet.SampleAt(position);
	if (m_world != nullptr)
	{
		const auto tile = m_world->SampleAt(PlanetaryCoordinates::ToCartesian(position));
		if (tile.has_value())
		{
			sample = ApplyTile(sample, &*tile);
		}
	}
	return sample;
}

PlanetFieldSample WorldTerrain::SamplePlanet(CartesianPoint3D direction) const noexcept
{
	return SampleAt(PlanetaryCoordinates::ToGeographic(direction));
}

double WorldTerrain::ElevationMeters(const GeographicCoordinates &position, double minWavelengthMeters) const noexcept
{
	return ElevationMeters(position, SampleAt(position), minWavelengthMeters);
}

double WorldTerrain::ElevationMeters(const GeographicCoordinates &position, const PlanetFieldSample &planet,
                                     double minWavelengthMeters) const noexcept
{
	return m_planet.ElevationMeters(position, planet, minWavelengthMeters);
}

double WorldTerrain::ElevationMeters(CartesianPoint3D direction, double minWavelengthMeters) const noexcept
{
	return ElevationMeters(PlanetaryCoordinates::ToGeographic(direction), minWavelengthMeters);
}

double WorldTerrain::ElevationMeters(CartesianPoint3D direction, const PlanetFieldSample &planet,
                                     double minWavelengthMeters) const noexcept
{
	return ElevationMeters(PlanetaryCoordinates::ToGeographic(direction), planet, minWavelengthMeters);
}
} // namespace Infinitesia
