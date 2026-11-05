#include "domains/natural/planetary/internal/SurfaceModel.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia::GenerationDetail
{

// --------------------------------------------------------------------------
// Soil helpers
// --------------------------------------------------------------------------
double SoilFertility(RockType rock, double tempC, double precipMm, double planetAgeFrac)
{
	double base = 0.2;
	switch (rock)
	{
	case RockType::VolcanicIgneous:
		base = 0.65;
		break;
	case RockType::BasalticOceanic:
		base = 0.35;
		break;
	case RockType::SedimentaryBasin:
		base = 0.55;
		break;
	case RockType::FoldedSedimentary:
		base = 0.45;
		break;
	case RockType::GraniticShield:
		base = 0.30;
		break;
	case RockType::MetamorphicBelt:
		base = 0.25;
		break;
	}
	const double tempFactor = std::clamp((tempC + 5.0) / 30.0, 0.0, 1.0);
	const double wetFactor = std::clamp(precipMm / 900.0, 0.0, 1.0);
	const double weathering = tempFactor * wetFactor;
	const double ageFactor = std::clamp(planetAgeFrac, 0.0, 1.0);
	const double leaching = (tempC > 22.0 && precipMm > 1500.0) ? 0.6 : 1.0;
	return std::clamp(base * (0.3 + 0.7 * weathering) * (0.5 + 0.5 * ageFactor) * leaching, 0.0, 1.0);
}

double SoilDepth(RockType rock, double precipMm, double slope, double planetAgeFrac)
{
	double base = 0.3;
	switch (rock)
	{
	case RockType::SedimentaryBasin:
		base = 0.8;
		break;
	case RockType::VolcanicIgneous:
		base = 0.5;
		break;
	case RockType::BasalticOceanic:
		base = 0.3;
		break;
	case RockType::GraniticShield:
		base = 0.4;
		break;
	case RockType::FoldedSedimentary:
		base = 0.5;
		break;
	case RockType::MetamorphicBelt:
		base = 0.25;
		break;
	}
	const double wetFactor = std::clamp(precipMm / 1200.0, 0.0, 1.0);
	const double slopeFactor = 1.0 - std::clamp(slope * 1.5, 0.0, 0.9);
	const double ageFactor = std::clamp(planetAgeFrac, 0.1, 1.0);
	return std::clamp(base * wetFactor * slopeFactor * ageFactor, 0.0, 1.0);
}

// --------------------------------------------------------------------------
// Biome classification — Köppen-extended, 25 types
// --------------------------------------------------------------------------
Biome ClassifyBiome(const PlanetRegion &r)
{
	const double elevKm = r.elevationNormalized * kMaxElevationKm;
	const double t = r.temperatureCelsius;
	const double p = r.precipitationMm;
	const double dry = r.drySeasonMonths;
	const double snow = r.snowDays;

	// Aquatic
	if (r.isOcean)
	{
		if (t < -2.0)
			return Biome::PolarSea;
		if (elevKm < -4.5)
			return Biome::DeepOcean;
		if (t > 20.0 && elevKm > -0.4)
			return Biome::CoralReef;
		return Biome::ShallowSea;
	}

	// Only a cell mostly covered by water reads as water; smaller lakes sit
	// inside an ordinary land biome and are reported via lakeFraction.
	if (r.isLake && r.lakeFraction >= 0.5)
		return Biome::ShallowSea;

	// Salt flat: hot endorheic basin, near-zero precip
	if (t > 15.0 && p < 80.0 && r.drainageAccumulation < 0.01)
		return Biome::SaltFlat;

	// Volcanic terrain
	if (r.isVolcanicZone && elevKm < 3.0 && t > -5.0 && p < 250.0)
		return Biome::VolcanicTerrain;

	// Alpine (>~2500 m)
	if (elevKm > 4.5)
		return (t < -5.0) ? Biome::Glacier : Biome::AlpineRock;
	if (elevKm > 2.5 && t < 6.0)
		return (snow > 150.0) ? Biome::Glacier : Biome::AlpineGrassland;

	// Polar / ice
	if (t < -20.0)
		return Biome::IceCap;
	if (t < -15.0)
		return Biome::Glacier;

	// Tundra
	if (t < -2.0)
		return (p > 350.0) ? Biome::WetTundra : Biome::Tundra;

	// Boreal taiga
	if (t < 7.0)
	{
		if (p < 250.0)
			return Biome::Tundra;
		if (r.soilMoisture > 0.7)
			return Biome::BorealWetland;
		return Biome::BorealForest;
	}

	// Cold desert / steppe
	if (t < 14.0 && p < 200.0)
		return Biome::ColdDesert;
	if (t < 14.0 && p < 450.0)
		return Biome::TemperateSteppe;

	// Temperate
	if (t < 18.0)
	{
		if (dry > 3.0 && p > 300.0 && p < 900.0)
			return Biome::MediterraneanShrub;
		if (p > 1500.0 && dry < 2.0)
			return Biome::TemperateRainforest;
		if (p > 700.0)
			return Biome::TemperateDeciduous;
		if (p > 400.0)
			return Biome::TemperateMixed;
		if (p > 200.0)
			return Biome::TemperateSteppe;
		return Biome::ColdDesert;
	}

	// Hot desert
	if (p < 200.0)
		return Biome::HotDesert;

	// Subtropical / tropical
	if (dry >= 2.0 && dry <= 7.0 && p > 1100.0)
		return Biome::TropicalMonsoonForest;
	if (dry > 4.0)
	{
		if (p > 800.0)
			return Biome::TropicalDryForest;
		if (p > 400.0)
			return Biome::Savanna;
		return Biome::HotDesert;
	}

	if (p > 1600.0)
		return Biome::TropicalRainforest;
	if (p > 900.0)
		return Biome::TropicalDryForest;
	if (p > 500.0)
		return Biome::Savanna;
	return Biome::HotDesert;
}

Biome ClassifyBiome(const PlanetSurface &surface, double elevationMeters)
{
	return ClassifyBiome(surface.ToRegion(elevationMeters));
}

} // namespace Infinitesia::GenerationDetail
