#pragma once

#include "domains/natural/planetary/GenerationScenario.h"
#include "domains/natural/planetary/PlanetParameters.h"

#include <cstdint>
#include <vector>

namespace Infinitesia
{
    struct GalacticContext
    {
        std::uint64_t id = 0;
        double systemAgeYears = 0.0;
        double metallicityDex = 0.0;
        double radiationEnvironment = 0.0;
    };

    struct Star
    {
        std::uint64_t id = 0;
        double massKilograms = 0.0;
        double ageYears = 0.0;
        double mainSequenceLifetimeYears = 0.0;
        double radiusMeters = 0.0;
        double luminosityWatts = 0.0;
        double effectiveTemperatureKelvin = 0.0;
        double metallicityDex = 0.0;
    };

    struct Orbit
    {
        double semiMajorAxisMeters = 0.0;
        double eccentricity = 0.0;
        double inclinationDegrees = 0.0;
        double orbitalPeriodSeconds = 0.0;
        double meanIncidentFluxInEarthUnits = 0.0;
    };

    struct ProtoplanetaryDisk
    {
        std::uint64_t id = 0;
        double totalMassKilograms = 0.0;
        double solidMassKilograms = 0.0;
        double metallicityDex = 0.0;
        double lifetimeYears = 0.0;
        double waterCondensationRadiusMeters = 0.0;
    };

    enum class GenerationEventType
    {
        SystemFormed,
        DiskFormed,
        PlanetAccreted,
        PlanetDifferentiated
    };

    struct GenerationEvent
    {
        std::uint64_t id = 0;
        GenerationEventType type = GenerationEventType::SystemFormed;
        double timeAfterSystemFormationYears = 0.0;
        std::uint64_t subjectId = 0;
        std::uint64_t parentEventId = 0;
    };

    struct StarSystem
    {
        std::uint64_t id = 0;
        GenerationScenario scenario{};
        GalacticContext galacticContext{};
        Star star{};
        ProtoplanetaryDisk disk{};
        Orbit planetOrbit{};
        PlanetParameters planet{};
        std::vector<GenerationEvent> events;
    };
}
