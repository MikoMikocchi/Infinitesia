#pragma once

#include "planet/Planet.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Infinitesia
{
    // -----------------------------------------------------------------------
    // Biome classification — Köppen-extended, 25 types
    // -----------------------------------------------------------------------
    enum class Biome : std::uint8_t
    {
        // Aquatic
        DeepOcean,
        ShallowSea,
        CoralReef,
        PolarSea,

        // Cold / polar
        IceCap,
        Glacier,
        Tundra,
        WetTundra,

        // Boreal
        BorealForest,
        BorealWetland,

        // Temperate
        TemperateDeciduous,
        TemperateMixed,
        TemperateRainforest,
        TemperateSteppe,
        MediterraneanShrub,

        // Dry
        ColdDesert,
        HotDesert,
        SaltFlat,

        // Tropical/subtropical
        Savanna,
        TropicalDryForest,
        TropicalMonsoonForest,
        TropicalRainforest,

        // Alpine
        AlpineGrassland,
        AlpineRock,

        // Special
        VolcanicTerrain,
    };

    // -----------------------------------------------------------------------
    // Surface rock type — simplified geological classification
    // -----------------------------------------------------------------------
    enum class RockType : std::uint8_t
    {
        BasalticOceanic,    // young oceanic crust
        GraniticShield,     // old stable continental craton
        SedimentaryBasin,   // deposited layers in stable basins
        VolcanicIgneous,    // fresh lava / igneous near hotspots
        MetamorphicBelt,    // high-pressure collision zone rocks
        FoldedSedimentary,  // mountain belts — uplifted and folded
    };

    // -----------------------------------------------------------------------
    // PlanetRegion — full planetary data for one grid cell
    // -----------------------------------------------------------------------
    struct PlanetRegion
    {
        // Grid position
        int latitudeIndex = 0;
        int longitudeIndex = 0;
        std::uint32_t seed = 0;
        double latitudeDegrees = 0.0;
        double longitudeDegrees = 0.0;

        // --- Tectonics ---
        int    tectonicPlateId = 0;
        double tectonicStress = 0.0;      // 0..1, proximity to plate boundary
        bool   isVolcanicZone = false;
        double tectonicReliefKm = 0.0;    // broad uplift / basin / ridge relief
        double continentalCrustFraction = 0.0; // 0=oceanic, 1=continental lithosphere
        double crustThickness = 0.0;      // km
        double isostaticCrustThicknessKm = 0.0; // smoothed background thickness for terrain
        double crustAge = 0.0;            // normalized 0..1 (0=brand new, 1=ancient)
        RockType surfaceRock = RockType::BasalticOceanic;

        // --- Terrain ---
        double elevationNormalized = 0.0; // [-1..1]: <0 = below sea level
        double slopeNormalized = 0.0;     // 0..1, steepness of terrain
        bool   isOcean = false;

        // --- Hydrology ---
        bool   hasRiver = false;
        double riverDischarge = 0.0;      // relative 0..1 (normalised)
        double annualRunoffVolumeKm3 = 0.0; // physical annual runoff volume [km³/yr]
        bool   isLake = false;
        double drainageAccumulation = 0.0; // watershed flow, area-weighted, normalised

        // --- Soil ---
        double soilFertility = 0.0;       // 0..1
        double soilDepthNorm = 0.0;       // 0..1 (0=bedrock, 1=deep soil)

        // --- Climate ---
        double temperatureCelsius = 0.0;
        double temperatureAnnualRangeC = 0.0; // difference winter/summer
        double precipitationMm = 0.0;
        std::array<double, 12> monthlyTemperatureCelsius{};
        std::array<double, 12> monthlyPrecipitationMm{};
        double drySeasonMonths = 0.0;         // months with precip < 30mm
        double humidityNormalized = 0.0;
        double soilMoisture = 0.0;            // 0..1
        double snowDays = 0.0;                // days per year with snow
        double continentality = 0.0;          // 0=maritime, 1=continental
        double windMoistureFactor = 1.0; // ocean fetch, windward uplift and lee drying

        // --- Classification ---
        Biome biome = Biome::DeepOcean;
    };

    // -----------------------------------------------------------------------
    // Subset of StarSystem data that drives planet generation
    // -----------------------------------------------------------------------
    struct ClimateInputs
    {
        double meanIncidentFluxInEarthUnits = 1.0;
        double axialTiltDegrees = 23.44;
        double atmospherePressureKPa = 101.325;
        double volatileMassFraction = 0.02;
        double gravityMetersPerSecondSquared = 9.81;
        double rotationPeriodHours = 24.0;      // affects Coriolis / circulation
        double planetAgeYears = 4.5e9;          // affects erosion, soil development
    };

    // -----------------------------------------------------------------------
    // PlanetMap — caches a fully-generated grid; GetRegion is O(1)
    // -----------------------------------------------------------------------
    class PlanetMap
    {
    public:
        explicit PlanetMap(PlanetParameters parameters = {}, ClimateInputs climate = {});

        const Planet&        GetPlanet()        const noexcept;
        const PlanetParameters& GetParameters() const noexcept;
        const ClimateInputs& GetClimateInputs() const noexcept;
        int GetLatitudeBandCount()    const noexcept;
        int GetLongitudeSegmentCount() const noexcept;

        const PlanetRegion& GetRegion(int latitudeIndex, int longitudeIndex) const noexcept;
        double SampleElevation(int latitudeIndex, int longitudeIndex) const;
        bool   IsOcean(int latitudeIndex, int longitudeIndex) const;

    private:
        Planet       m_planet;
        ClimateInputs m_climate;
        std::vector<PlanetRegion> m_grid; // row-major [lat * lonSegments + lon]

        // Generation stages (called in constructor)
        void GenerateTectonics();
        void GenerateTerrain();
        void GenerateHydrology();
        void GenerateClimate();
        void GenerateSoils();
        void ClassifyBiomes();

        // Helpers
        PlanetRegion&       Cell(int lat, int lon);
        const PlanetRegion& Cell(int lat, int lon) const;
        int LatBands()  const noexcept;
        int LonSegs()   const noexcept;
    };
}
