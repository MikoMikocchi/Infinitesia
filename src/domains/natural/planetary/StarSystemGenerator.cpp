#include "domains/natural/planetary/StarSystemGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <stdexcept>

namespace Infinitesia
{
namespace
{
constexpr double kAstronomicalUnitMeters = 149597870700.0;
constexpr double kEarthMassKilograms = 5.9722e24;
constexpr double kSolarMassKilograms = 1.98847e30;
constexpr double kSolarRadiusMeters = 6.957e8;
constexpr double kSolarLuminosityWatts = 3.828e26;
constexpr double kSolarEffectiveTemperatureKelvin = 5772.0;
constexpr double kGravitationalConstant = 6.67430e-11;

std::uint64_t Mix(std::uint64_t value)
{
	value += 0x9e3779b97f4a7c15ull;
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
	return value ^ (value >> 31);
}

std::uint64_t DeriveSeed(std::uint64_t seed, std::uint64_t stage)
{
	return Mix(seed ^ Mix(stage));
}

double UnitInterval(std::uint64_t value)
{
	constexpr double kInverse53BitRange = 1.0 / 9007199254740992.0;
	return static_cast<double>(Mix(value) >> 11) * kInverse53BitRange;
}

double Sample(std::uint64_t seed, std::uint64_t stage, double minimum, double maximum)
{
	return minimum + (maximum - minimum) * UnitInterval(DeriveSeed(seed, stage));
}

std::uint64_t StableId(std::uint64_t seed, std::uint64_t stage)
{
	return DeriveSeed(seed, stage);
}

void ValidateScenario(const GenerationScenario &scenario)
{
	if (!scenario.IsValid())
	{
		throw std::invalid_argument("Generation scenario contains an unsupported preset or version.");
	}
}

void ValidateGeneratedSystem(const StarSystem &system)
{
	if (system.id == 0 || system.galacticContext.id == 0 || system.star.id == 0 || system.disk.id == 0 ||
	    system.planet.planetId == 0 || !std::isfinite(system.galacticContext.systemAgeYears) ||
	    system.galacticContext.systemAgeYears <= 0.0 || !std::isfinite(system.star.massKilograms) ||
	    system.star.massKilograms <= 0.0 || !std::isfinite(system.star.ageYears) || system.star.ageYears <= 0.0 ||
	    !std::isfinite(system.star.mainSequenceLifetimeYears) ||
	    system.star.ageYears >= system.star.mainSequenceLifetimeYears || !std::isfinite(system.star.radiusMeters) ||
	    system.star.radiusMeters <= 0.0 || !std::isfinite(system.star.luminosityWatts) ||
	    system.star.luminosityWatts <= 0.0 || !std::isfinite(system.star.effectiveTemperatureKelvin) ||
	    system.star.effectiveTemperatureKelvin <= 0.0 || !std::isfinite(system.disk.totalMassKilograms) ||
	    system.disk.totalMassKilograms <= 0.0 || !std::isfinite(system.disk.solidMassKilograms) ||
	    system.disk.solidMassKilograms <= 0.0 || system.disk.solidMassKilograms >= system.disk.totalMassKilograms ||
	    !std::isfinite(system.disk.lifetimeYears) || system.disk.lifetimeYears <= 0.0 ||
	    !std::isfinite(system.planetOrbit.semiMajorAxisMeters) || system.planetOrbit.semiMajorAxisMeters <= 0.0 ||
	    !std::isfinite(system.planetOrbit.eccentricity) || system.planetOrbit.eccentricity < 0.0 ||
	    system.planetOrbit.eccentricity >= 1.0 || !std::isfinite(system.planetOrbit.orbitalPeriodSeconds) ||
	    system.planetOrbit.orbitalPeriodSeconds <= 0.0 ||
	    !std::isfinite(system.planetOrbit.meanIncidentFluxInEarthUnits) ||
	    system.planetOrbit.meanIncidentFluxInEarthUnits <= 0.0 || !system.planet.IsValid() ||
	    !std::isfinite(system.planet.ageYears) || system.planet.ageYears <= 0.0 ||
	    system.planet.ageYears >= system.galacticContext.systemAgeYears)
	{
		throw std::runtime_error("Generated star system contains an invalid state value.");
	}

	const double expectedDensity =
	    system.planet.massKilograms / ((4.0 / 3.0) * std::numbers::pi * std::pow(system.planet.radiusMeters, 3.0));
	const double expectedGravity =
	    kGravitationalConstant * system.planet.massKilograms / std::pow(system.planet.radiusMeters, 2.0);
	const double expectedFlux = (system.star.luminosityWatts / kSolarLuminosityWatts) /
	                            (std::pow(system.planetOrbit.semiMajorAxisMeters / kAstronomicalUnitMeters, 2.0) *
	                             std::sqrt(1.0 - std::pow(system.planetOrbit.eccentricity, 2.0)));
	const double densityError =
	    std::abs(system.planet.meanDensityKilogramsPerCubicMeter - expectedDensity) / expectedDensity;
	const double gravityError =
	    std::abs(system.planet.gravityMetersPerSecondSquared - expectedGravity) / expectedGravity;

	if (densityError > 1e-10 || gravityError > 1e-10 ||
	    std::abs(system.planetOrbit.meanIncidentFluxInEarthUnits - expectedFlux) > expectedFlux * 1e-10 ||
	    system.star.ageYears != system.galacticContext.systemAgeYears || system.events.empty())
	{
		throw std::runtime_error("Generated star system failed physical consistency validation.");
	}

	for (std::size_t index = 0; index < system.events.size(); ++index)
	{
		const GenerationEvent &event = system.events[index];
		if (event.id == 0 || !std::isfinite(event.timeAfterSystemFormationYears) ||
		    event.timeAfterSystemFormationYears < 0.0 ||
		    (index > 0 && system.events[index - 1].timeAfterSystemFormationYears > event.timeAfterSystemFormationYears))
		{
			throw std::runtime_error("Generated star system has an invalid event timeline.");
		}

		if (event.parentEventId != 0)
		{
			const auto parent = std::find_if(
			    system.events.begin(), system.events.begin() + static_cast<std::ptrdiff_t>(index),
			    [&event](const GenerationEvent &candidate) { return candidate.id == event.parentEventId; });
			if (parent == system.events.begin() + static_cast<std::ptrdiff_t>(index))
			{
				throw std::runtime_error("Generated star system has an event with a missing or later "
				                         "parent.");
			}
		}
	}
}
} // namespace

StarSystem StarSystemGenerator::Generate(const GenerationScenario &scenario)
{
	ValidateScenario(scenario);

	constexpr std::uint64_t kContextStage = 0x434f4e5445585401ull;
	constexpr std::uint64_t kSystemStage = 0x53595354454d0001ull;
	constexpr std::uint64_t kStarStage = 0x5354415200000002ull;
	constexpr std::uint64_t kDiskStage = 0x4449534b00000003ull;
	[[maybe_unused]] constexpr std::uint64_t kOrbitStage = 0x4f52424954000004ull;
	constexpr std::uint64_t kPlanetStage = 0x504c414e455405ull;
	constexpr std::uint64_t kFormationStage = 0x464f524d415406ull;
	constexpr std::uint64_t kSystemEventStage = 0x4556454e540001ull;
	constexpr std::uint64_t kDiskEventStage = 0x4556454e540002ull;
	constexpr std::uint64_t kAccretionEventStage = 0x4556454e540003ull;
	constexpr std::uint64_t kDifferentiationEventStage = 0x4556454e540004ull;

	const std::uint64_t generationSeed = Mix(scenario.seed ^ static_cast<std::uint64_t>(scenario.generatorVersion));
	StarSystem system;
	system.scenario = scenario;
	system.id = StableId(generationSeed, kSystemStage);

	GalacticContext &context = system.galacticContext;
	context.id = StableId(generationSeed, kContextStage);
	context.metallicityDex = Sample(generationSeed, 0x1002ull, -0.25, 0.25);
	context.radiationEnvironment = Sample(generationSeed, 0x1003ull, 0.0, 1.0);

	Star &star = system.star;
	star.id = StableId(generationSeed, kStarStage);
	const double maximumViableMassSolar = std::pow(0.75e10 / scenario.constraints.minimumSystemAgeYears, 1.0 / 2.5);
	const double maximumSupportedMassSolar =
	    std::min(scenario.constraints.maximumStarMassSolar, maximumViableMassSolar);
	if (maximumSupportedMassSolar < scenario.constraints.minimumStarMassSolar)
	{
		throw std::invalid_argument("Generation constraints cannot produce a star that survives to the "
		                            "minimum system age.");
	}
	star.massKilograms =
	    Sample(generationSeed, 0x2001ull, scenario.constraints.minimumStarMassSolar, maximumSupportedMassSolar) *
	    kSolarMassKilograms;
	star.metallicityDex = context.metallicityDex;
	star.mainSequenceLifetimeYears = 1.0e10 * std::pow(star.massKilograms / kSolarMassKilograms, -2.5);
	context.systemAgeYears =
	    Sample(generationSeed, 0x1001ull, scenario.constraints.minimumSystemAgeYears,
		       std::min(scenario.constraints.maximumSystemAgeYears, star.mainSequenceLifetimeYears * 0.75));
	star.ageYears = context.systemAgeYears;
	const double massInSolarUnits = star.massKilograms / kSolarMassKilograms;
	const double ageFraction = star.ageYears / star.mainSequenceLifetimeYears;
	const double luminosityInSolarUnits = std::pow(massInSolarUnits, 3.5) * (1.0 + 0.4 * ageFraction);
	star.luminosityWatts = luminosityInSolarUnits * kSolarLuminosityWatts;
	star.radiusMeters = std::pow(massInSolarUnits, 0.8) * kSolarRadiusMeters * (1.0 + 0.1 * ageFraction);
	star.effectiveTemperatureKelvin = kSolarEffectiveTemperatureKelvin * std::pow(luminosityInSolarUnits, 0.25) /
	                                  std::sqrt(star.radiusMeters / kSolarRadiusMeters);

	ProtoplanetaryDisk &disk = system.disk;
	disk.id = StableId(generationSeed, kDiskStage);
	const double diskMassRatio = Sample(generationSeed, 0x3001ull, 0.005, 0.02);
	const double metallicityScale = std::pow(10.0, context.metallicityDex);
	disk.totalMassKilograms = star.massKilograms * diskMassRatio;
	disk.solidMassKilograms = disk.totalMassKilograms * 0.01 * metallicityScale;
	disk.metallicityDex = context.metallicityDex;
	disk.lifetimeYears = Sample(generationSeed, 0x3002ull, 2.0e6, 8.0e6);
	const double diskTemperatureAtOneAu = 280.0 * std::pow(luminosityInSolarUnits, 0.25);
	disk.waterCondensationRadiusMeters = kAstronomicalUnitMeters * std::pow(diskTemperatureAtOneAu / 170.0, 2.0);

	Orbit &orbit = system.planetOrbit;
	orbit.meanIncidentFluxInEarthUnits =
	    Sample(generationSeed, 0x4001ull, scenario.constraints.minimumIncidentFluxInEarthUnits,
		       scenario.constraints.maximumIncidentFluxInEarthUnits);
	orbit.eccentricity = Sample(generationSeed, 0x4002ull, 0.0, 0.08);
	orbit.semiMajorAxisMeters =
	    kAstronomicalUnitMeters *
	    std::sqrt(luminosityInSolarUnits /
		          (orbit.meanIncidentFluxInEarthUnits * std::sqrt(1.0 - std::pow(orbit.eccentricity, 2.0))));
	orbit.inclinationDegrees = Sample(generationSeed, 0x4003ull, 0.0, 3.0);
	orbit.orbitalPeriodSeconds =
	    2.0 * std::numbers::pi *
	    std::sqrt(std::pow(orbit.semiMajorAxisMeters, 3.0) / (kGravitationalConstant * star.massKilograms));

	PlanetParameters &planet = system.planet;
	planet.seed = static_cast<std::uint32_t>(DeriveSeed(generationSeed, kPlanetStage));
	planet.planetId = StableId(generationSeed, kPlanetStage);
	planet.massKilograms = std::min(disk.solidMassKilograms * Sample(generationSeed, 0x5001ull, 0.10, 0.22),
	                                scenario.constraints.maximumPlanetMassEarths * kEarthMassKilograms);
	planet.bulkIronMassFraction = Sample(generationSeed, 0x5002ull, 0.28, 0.36);
	planet.meanDensityKilogramsPerCubicMeter = 4400.0 + planet.bulkIronMassFraction * 3500.0;
	planet.radiusMeters =
	    std::cbrt(planet.massKilograms / ((4.0 / 3.0) * std::numbers::pi * planet.meanDensityKilogramsPerCubicMeter));
	planet.gravityMetersPerSecondSquared =
	    kGravitationalConstant * planet.massKilograms / std::pow(planet.radiusMeters, 2.0);
	planet.rotationHours = Sample(generationSeed, 0x5003ull, 12.0, 48.0);
	planet.axialTiltDegrees = Sample(generationSeed, 0x5004ull, 0.0, 45.0);
	const double planetFormationTime = Sample(generationSeed, kFormationStage, 1.0e6, disk.lifetimeYears);
	const double planetDifferentiationTime = planetFormationTime + 1.0e5;
	planet.ageYears = context.systemAgeYears - planetDifferentiationTime;
	const double volatileAvailability = orbit.semiMajorAxisMeters >= disk.waterCondensationRadiusMeters ? 0.08 : 0.015;
	planet.volatileMassFraction = volatileAvailability * Sample(generationSeed, 0x5005ull, 0.5, 1.5);
	const double volatileRetention =
	    std::clamp(0.95 + 0.1 * (std::sqrt(planet.gravityMetersPerSecondSquared / 9.81) - 1.0) -
		               0.08 * context.radiationEnvironment,
		           0.8, 1.05);
	const double volatilePressureScale = std::clamp(std::sqrt(planet.volatileMassFraction / 0.015), 1.0, 1.15);
	planet.atmospherePressureKPa = std::clamp(101.325 * volatilePressureScale * volatileRetention, 5.0, 300.0);

	const std::uint64_t systemEventId = StableId(generationSeed, kSystemEventStage);
	const std::uint64_t diskEventId = StableId(generationSeed, kDiskEventStage);
	const std::uint64_t accretionEventId = StableId(generationSeed, kAccretionEventStage);
	system.events = {
	    {systemEventId, GenerationEventType::SystemFormed, 0.0, system.id, 0},
	    {diskEventId, GenerationEventType::DiskFormed, 1.0e5, disk.id, systemEventId},
	    {accretionEventId, GenerationEventType::PlanetAccreted, planetFormationTime, planet.planetId, diskEventId},
	    {StableId(generationSeed, kDifferentiationEventStage), GenerationEventType::PlanetDifferentiated,
		 planetDifferentiationTime, planet.planetId, accretionEventId}};

	ValidateGeneratedSystem(system);
	return system;
}
} // namespace Infinitesia
