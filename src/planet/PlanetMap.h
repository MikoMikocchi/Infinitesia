#pragma once

#include "core/LoadProgress.h"
#include "planet/Planet.h"
#include "planet/StarSystem.h"
#include "planet/geology/PlanetEpochs.h"
#include "planet/geology/Province.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Infinitesia
{
    // PlanetRegion::elevationNormalized spans [-1..1] over +/- this height [km].
    inline constexpr double kMaxElevationKm = 9.0;

    // Version of the geological data a PlanetMap carries (boundary context, crust age,
    // sediment, heat flow, provinces). Kept apart from the generator version: adding
    // these fields leaves every older field of the map unchanged. Raise it whenever a
    // change alters any geological field; saves record it next to the planet seed.
    inline constexpr std::uint32_t kPlanetGeologyDataVersion = 1;

    // Plate boundary within kBoundaryReachKm of a cell. Order-independent of the
    // generator's internal classification.
    inline constexpr double kBoundaryReachKm = 1000.0;

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
    // Kind of the nearest plate boundary (see PlanetRegion::boundarySubtype)
    // -----------------------------------------------------------------------
    enum class PlateBoundaryKind : std::uint8_t
    {
        None,                   // no boundary within kBoundaryReachKm
        MidOceanRidge,
        ContinentalRift,
        OceanicSubduction,      // both sides oceanic: trench plus island arc
        ContinentalSubduction,  // oceanic plate under a continent: trench plus continental arc
        ContinentalCollision,
        TransformFault,
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

        // --- Geological context (additive; the fields above are unchanged) ---
        // Nearest plate boundary: its kind (None beyond kBoundaryReachKm), the distance
        // to it and which side of it the cell is on. The distance is half the gap between
        // the two nearest plate centres, an approximation good to about a cell.
        PlateBoundaryKind boundarySubtype = PlateBoundaryKind::None;
        double distanceToBoundaryKm = 0.0;
        bool   isSubductingPlate = false;  // down-going side of a subduction boundary
        // Motion of the cell's plate over the planet's rigid sphere, and of the cell's
        // plate relative to the neighbouring plate across the nearest boundary.
        double plateVelocityEastCmPerYear = 0.0;
        double plateVelocityNorthCmPerYear = 0.0;
        double boundaryRelativeVelocityEastCmPerYear = 0.0;
        double boundaryRelativeVelocityNorthCmPerYear = 0.0;
        double crustAgeMyr = 0.0;         // absolute age of the crust, million years
        double sedimentThicknessKm = 0.0; // basin, shelf and abyssal sediment cover
        double heatFlowMwPerM2 = 0.0;     // surface heat flow
        double hotspotInfluence = 0.0;    // 0..1, mantle-plume activity away from boundaries
        double paleoLatitudeDegrees = 0.0; // reserved: equals latitudeDegrees until plates are reconstructed
        ProvinceId provinceId = kNoProvince;

        // --- Terrain ---
        double elevationNormalized = 0.0; // [-1..1]: <0 = below sea level
        double slopeNormalized = 0.0;     // 0..1, steepness of terrain
        bool   isOcean = false;

        // --- Hydrology ---
        bool   hasRiver = false;
        double riverDischarge = 0.0;      // relative 0..1 (normalised)
        double riverDischargeM3PerSecond = 0.0; // physical mean discharge [m³/s]
        double annualRunoffVolumeKm3 = 0.0; // physical annual runoff volume [km³/yr]
        bool   isLake = false;
        double drainageAccumulation = 0.0; // watershed flow, area-weighted, normalised
        double lakeFraction = 0.0;         // share of the cell covered by water [0..1]
        double lakeSurfaceAreaKm2 = 0.0;   // water surface, <= cell area
        double lakeWaterVolumeKm3 = 0.0;
        double lakeAnnualInflowKm3 = 0.0;       // [km³/yr]
        double lakeAnnualEvaporationKm3 = 0.0;  // [km³/yr]
        double lakeAnnualInfiltrationKm3 = 0.0; // [km³/yr]
        double lakeAnnualSpillKm3 = 0.0;        // [km³/yr]
        double lakeWaterLevelKm = 0.0;          // elevation above sea level [km]
        double lakeSpillElevationKm = 0.0;      // elevation above sea level [km]
        double lakeSpillHeightKm = 0.0;         // basin floor to spill level [km]

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

    // The climate inputs of a generated system (what the game builds its planet map from).
    ClimateInputs ClimateInputsFor(const StarSystem& system);

    // -----------------------------------------------------------------------
    // Planet-wide annual freshwater ledger produced by hydrology (km^3/yr).
    //   oceanDischarge + landSink == landRunoff + lakePrecipitationGain
    //                                - lakeLoss - drySinkLoss
    // -----------------------------------------------------------------------
    struct PlanetWaterBalance
    {
        double landRunoffKm3 = 0.0;           // sum of local runoff over land
        double lakePrecipitationGainKm3 = 0.0; // rain on lake surfaces beyond the runoff it replaces
        double lakeLossKm3 = 0.0;             // evaporation + infiltration kept by lakes
        double drySinkLossKm3 = 0.0;          // inflow of sinks too small to hold a lake
        double oceanDischargeKm3 = 0.0;       // total delivered into ocean cells
        double landSinkKm3 = 0.0;             // left in land cells with no outlet
    };

    // -----------------------------------------------------------------------
    // PlanetMap — caches a fully-generated grid; GetRegion is O(1)
    // -----------------------------------------------------------------------
    class PlanetMap
    {
    public:
        explicit PlanetMap(PlanetParameters parameters = {}, ClimateInputs climate = {},
                           LoadProgress* progress = nullptr);

        const Planet&        GetPlanet()        const noexcept;
        const PlanetParameters& GetParameters() const noexcept;
        const ClimateInputs& GetClimateInputs() const noexcept;
        int GetLatitudeBandCount()    const noexcept;
        int GetLongitudeSegmentCount() const noexcept;

        const PlanetRegion& GetRegion(int latitudeIndex, int longitudeIndex) const noexcept;
        double SampleElevation(int latitudeIndex, int longitudeIndex) const;
        bool   IsOcean(int latitudeIndex, int longitudeIndex) const;
        const PlanetWaterBalance& GetWaterBalance() const noexcept { return m_waterBalance; }

        // Geological provinces of the planet, indexed by ProvinceId - 1.
        const std::vector<Province>& GetProvinces() const noexcept { return m_provinces; }
        // nullptr for kNoProvince or an unknown id.
        const Province* FindProvince(ProvinceId id) const noexcept;
        const Province* FindProvince(const PlanetRegion& region) const noexcept { return FindProvince(region.provinceId); }
        const PlanetEpochTable& GetEpochs() const noexcept { return m_epochs; }

    private:
        PlanetWaterBalance m_waterBalance;
        std::vector<Province> m_provinces;
        PlanetEpochTable m_epochs;
        Planet       m_planet;
        ClimateInputs m_climate;
        std::vector<PlanetRegion> m_grid; // row-major [lat * lonSegments + lon]

        // Generation stages (called in constructor)
        void GenerateTectonics();
        void GenerateTerrain();
        void GenerateGeology();   // heat flow, sediment, hotspots, provinces (after terrain)
        void GenerateProvinces(const std::vector<double>& distanceToOceanKm); // called by GenerateGeology
        void GenerateHydrology();
        void GenerateClimate();
        void GenerateSoils();
        void ClassifyBiomes();

        // Optional load progress sink; reports how many rows of the current step are done.
        void ReportRow(int row, int rowCount) const;
        LoadProgress* m_progress = nullptr;

        // Helpers
        PlanetRegion&       Cell(int lat, int lon);
        const PlanetRegion& Cell(int lat, int lon) const;
        int LatBands()  const noexcept;
        int LonSegs()   const noexcept;
    };
}
