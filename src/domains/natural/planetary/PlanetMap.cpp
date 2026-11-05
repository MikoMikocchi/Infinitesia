// PlanetMap.cpp
// PlanetMap construction and accessors; the generation itself is in stages/.

#include "domains/natural/planetary/PlanetMap.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia
{

// ==========================================================================
// PlanetMap — construction (runs all stages)
// ==========================================================================

ClimateInputs ClimateInputsFor(const StarSystem &system)
{
	return {
	    system.planetOrbit.meanIncidentFluxInEarthUnits,
	    system.planet.axialTiltDegrees,
	    system.planet.atmospherePressureKPa,
	    system.planet.volatileMassFraction,
	    system.planet.gravityMetersPerSecondSquared,
	    system.planet.rotationHours,
	    system.planet.ageYears,
	};
}

PlanetMap::PlanetMap(PlanetParameters parameters, ClimateInputs climate, LoadProgress *progress)
    : PlanetMap(std::move(parameters), climate, MakeDefaultPlanetPipeline(), progress)
{
}

PlanetMap::PlanetMap(PlanetParameters parameters, ClimateInputs climate, const PlanetPipeline &pipeline,
                     LoadProgress *progress)
    : m_data{Planet(std::move(parameters)), climate, {}, {}, {}, {}, {}}
{
	PlanetBuild build(m_data, progress);
	const int latB = build.LatBands();
	const int lonS = build.LonSegs();
	m_data.grid.resize(static_cast<std::size_t>(latB) * static_cast<std::size_t>(lonS));

	for (int la = 0; la < latB; ++la)
	{
		for (int lo = 0; lo < lonS; ++lo)
		{
			auto &c = build.Cell(la, lo);
			const auto ctr = m_data.planet.GetRegionCenter(la, lo);
			c.latitudeIndex = la;
			c.longitudeIndex = lo;
			c.seed = m_data.planet.DeriveRegionSeed(la, lo);
			c.latitudeDegrees = ctr.latitudeDegrees;
			c.longitudeDegrees = ctr.longitudeDegrees;
		}
	}

	pipeline.Run(build);
}

// --------------------------------------------------------------------------
// Accessors
// --------------------------------------------------------------------------

const Planet &PlanetMap::GetPlanet() const noexcept
{
	return m_data.planet;
}
const PlanetParameters &PlanetMap::GetParameters() const noexcept
{
	return m_data.planet.GetParameters();
}
const ClimateInputs &PlanetMap::GetClimateInputs() const noexcept
{
	return m_data.climate;
}
int PlanetMap::GetLatitudeBandCount() const noexcept
{
	return m_data.planet.GetParameters().latitudeBands;
}
int PlanetMap::GetLongitudeSegmentCount() const noexcept
{
	return m_data.planet.GetParameters().longitudeSegments;
}

const PlanetRegion &PlanetMap::GetRegion(int la, int lo) const noexcept
{
	const int lonSegs = GetLongitudeSegmentCount();
	la = std::clamp(la, 0, GetLatitudeBandCount() - 1);
	lo = std::clamp(lo, 0, lonSegs - 1);
	return m_data.grid[static_cast<std::size_t>(la) * lonSegs + lo];
}

double PlanetMap::SampleElevation(int la, int lo) const
{
	return GetRegion(la, lo).elevationNormalized;
}
bool PlanetMap::IsOcean(int la, int lo) const
{
	return GetRegion(la, lo).isOcean;
}

const Province *PlanetMap::FindProvince(ProvinceId id) const noexcept
{
	if (id == kNoProvince || id > m_data.provinces.size())
	{
		return nullptr;
	}
	return &m_data.provinces[id - 1];
}

} // namespace Infinitesia
