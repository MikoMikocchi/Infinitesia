#pragma once

#include <cmath>
#include <cstdint>

namespace Infinitesia
{
enum class PlanetaryPreset
{
	RockyTemperate
};

struct GenerationConstraints
{
	double minimumStarMassSolar = 0.85;
	double maximumStarMassSolar = 1.15;
	double minimumSystemAgeYears = 2.0e9;
	double maximumSystemAgeYears = 8.0e9;
	double minimumIncidentFluxInEarthUnits = 0.75;
	double maximumIncidentFluxInEarthUnits = 1.25;
	double maximumPlanetMassEarths = 1.25;

	bool IsValid() const noexcept
	{
		return std::isfinite(minimumStarMassSolar) && std::isfinite(maximumStarMassSolar) &&
		       minimumStarMassSolar >= 0.75 && maximumStarMassSolar <= 1.25 &&
		       minimumStarMassSolar < maximumStarMassSolar && std::isfinite(minimumSystemAgeYears) &&
		       std::isfinite(maximumSystemAgeYears) && minimumSystemAgeYears > 0.0 &&
		       minimumSystemAgeYears < maximumSystemAgeYears && maximumSystemAgeYears <= 13.8e9 &&
		       std::isfinite(minimumIncidentFluxInEarthUnits) && std::isfinite(maximumIncidentFluxInEarthUnits) &&
		       minimumIncidentFluxInEarthUnits > 0.0 &&
		       minimumIncidentFluxInEarthUnits < maximumIncidentFluxInEarthUnits &&
		       maximumIncidentFluxInEarthUnits <= 3.0 && std::isfinite(maximumPlanetMassEarths) &&
		       maximumPlanetMassEarths > 0.0 && maximumPlanetMassEarths <= 3.0;
	}
};

struct GenerationScenario
{
	std::uint64_t seed = 1;
	std::uint32_t generatorVersion = 6;
	PlanetaryPreset preset = PlanetaryPreset::RockyTemperate;
	GenerationConstraints constraints{};

	bool IsValid() const noexcept
	{
		return generatorVersion > 0 && preset == PlanetaryPreset::RockyTemperate && constraints.IsValid();
	}
};
} // namespace Infinitesia
