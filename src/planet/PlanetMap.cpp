#include "planet/PlanetMap.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia
{
    namespace
    {
        // -----------------------------------------------------------------------
        // Deterministic hashing
        // -----------------------------------------------------------------------

        std::uint32_t Hash32(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x85ebca6bu;
            value ^= value >> 13;
            value *= 0xc2b2ae3du;
            value ^= value >> 16;
            return value;
        }

        double HashToUnit(std::uint32_t value)
        {
            return static_cast<double>(Hash32(value & 0xffffffffu)) /
                   static_cast<double>(0xffffffffu);
        }

        // -----------------------------------------------------------------------
        // Interpolation helpers
        // -----------------------------------------------------------------------

        double SmoothStep(double value)
        {
            return value * value * (3.0 - 2.0 * value);
        }

        double SmootherStep(double value)
        {
            // Ken Perlin's improved quintic
            return value * value * value * (value * (value * 6.0 - 15.0) + 10.0);
        }

        double Lerp(double a, double b, double t)
        {
            return a + (b - a) * std::clamp(t, 0.0, 1.0);
        }

        // -----------------------------------------------------------------------
        // Noise
        // -----------------------------------------------------------------------

        double ValueNoise2D(
            double latitude,
            double longitude,
            int latitudeCells,
            int longitudeCells,
            std::uint32_t seed)
        {
            const double latitudePosition = std::clamp(latitude, 0.0, 1.0) * latitudeCells;
            const double longitudePosition = (longitude - std::floor(longitude)) * longitudeCells;
            const int latitude0 = std::min(static_cast<int>(latitudePosition), latitudeCells);
            const int latitude1 = std::min(latitude0 + 1, latitudeCells);
            const int longitude0 = static_cast<int>(longitudePosition) % longitudeCells;
            const int longitude1 = (longitude0 + 1) % longitudeCells;
            const double latitudeBlend = SmootherStep(latitudePosition - latitude0);
            const double longitudeBlend = SmootherStep(longitudePosition - std::floor(longitudePosition));

            const auto sample = [seed](int latCell, int lonCell)
            {
                const std::uint32_t v =
                    seed ^
                    (static_cast<std::uint32_t>(latCell + 1) * 0x9e3779b9u) ^
                    (static_cast<std::uint32_t>(lonCell + 1) * 0x85ebca6bu);
                return HashToUnit(v);
            };

            const double nw = sample(latitude0, longitude0);
            const double ne = sample(latitude0, longitude1);
            const double sw = sample(latitude1, longitude0);
            const double se = sample(latitude1, longitude1);
            const double north = nw + (ne - nw) * longitudeBlend;
            const double south = sw + (se - sw) * longitudeBlend;
            return north + (south - north) * latitudeBlend;
        }

        // Fractal Brownian Motion: 5 octaves for richer terrain
        double FractalNoise2D(
            double latitude,
            double longitude,
            std::uint32_t seed,
            int baseLatCells,
            int baseLonCells)
        {
            double result = 0.0;
            double amplitude = 1.0;
            double totalAmplitude = 0.0;
            std::uint32_t octaveSeed = seed;
            int latCells = baseLatCells;
            int lonCells = baseLonCells;

            for (int octave = 0; octave < 5; ++octave)
            {
                result += ValueNoise2D(latitude, longitude, latCells, lonCells, octaveSeed) * amplitude;
                totalAmplitude += amplitude;
                amplitude *= 0.5;
                latCells *= 2;
                lonCells *= 2;
                octaveSeed = Hash32(octaveSeed ^ 0xdeadbeef);
            }

            return result / totalAmplitude;
        }

        double SmoothNoise2D(
            int latitudeIndex,
            int longitudeIndex,
            int latitudeBands,
            int longitudeSegments,
            std::uint32_t seed)
        {
            const double latitude = (static_cast<double>(latitudeIndex) + 0.5) / latitudeBands;
            const double longitude = (static_cast<double>(longitudeIndex) + 0.5) / longitudeSegments;
            return FractalNoise2D(latitude, longitude, seed, 6, 12);
        }

        // Separate noise field for moisture / humidity transport
        double MoistureNoise2D(
            int latitudeIndex,
            int longitudeIndex,
            int latitudeBands,
            int longitudeSegments,
            std::uint32_t seed)
        {
            const double latitude = (static_cast<double>(latitudeIndex) + 0.5) / latitudeBands;
            const double longitude = (static_cast<double>(longitudeIndex) + 0.5) / longitudeSegments;
            return FractalNoise2D(latitude, longitude, seed ^ 0xfeedface, 5, 10);
        }

        double ClampNormalized(double value)
        {
            return std::clamp(value, -1.0, 1.0);
        }

        // -----------------------------------------------------------------------
        // Climate model helpers
        // -----------------------------------------------------------------------

        // Annual mean insolation at a given latitude, accounting for axial tilt.
        // Simplified closed-form approximation of the real integral over solar geometry.
        // Returns fraction relative to sub-solar point maximum (0..1).
        double LatitudeInsolationFraction(double latitudeDegrees, double axialTiltDegrees)
        {
            const double latRad = latitudeDegrees * std::numbers::pi / 180.0;
            const double tiltRad = axialTiltDegrees * std::numbers::pi / 180.0;

            // Polar night: if |lat| > 90 - tilt, insolation approaches zero on one pole
            // We use a smooth approximation that blends between equatorial and polar regions.
            const double coLat = std::abs(latRad);

            // Cosine of latitude gives basic insolation shape
            // With nonzero tilt, the poles receive some annual-average insolation:
            //   I(phi) ~ cos(phi) * sqrt(1 - sin^2(phi)*sin^2(tilt))  [approximate]
            // We use a softened version to avoid exact zeros at poles.
            const double sinLat = std::sin(coLat);
            const double sinTilt = std::sin(tiltRad);

            // Annual mean approximation after spherical geometry averaging
            const double polarBlend = sinTilt;
            const double equatorBlend = 1.0 - polarBlend;
            const double cosLatitudeInsolation = std::max(0.0, std::cos(coLat));

            // Tilt redistributes some energy toward poles in summer, increasing annual mean
            // We blend between pure cosine (0-tilt) and a softer polar boost
            const double tiltBoost = std::max(0.0, 1.0 - sinLat * sinLat * (1.0 - sinTilt * sinTilt));
            const double insolation = equatorBlend * cosLatitudeInsolation + polarBlend * std::sqrt(tiltBoost) * 0.5;

            return std::clamp(insolation, 0.0, 1.0);
        }

        // Stefan-Boltzmann equilibrium temperature (K) from flux in Earth units.
        // Uses a simple energy balance: T = T_earth * (S/S0)^0.25
        // Earth average ~288K at flux=1.0 with typical albedo+greenhouse.
        double EquilibriumTemperatureKelvin(double fluxInEarthUnits)
        {
            constexpr double kEarthMeanTempKelvin = 288.0;
            return kEarthMeanTempKelvin * std::pow(std::max(0.01, fluxInEarthUnits), 0.25);
        }

        // Atmospheric greenhouse warming (K) from pressure.
        // Logarithmic: similar to CO2 forcing dependence.
        // Earth has ~30K greenhouse at 101 kPa.
        double GreenhouseWarming(double pressureKPa, double volatileMassFraction)
        {
            if (pressureKPa <= 0.0) return 0.0;
            constexpr double kEarthGreenhouseKelvin = 30.0;
            constexpr double kEarthPressureKPa = 101.325;
            // Volatile fraction affects CO2/H2O content which drives greenhouse
            const double volatileScale = std::clamp(volatileMassFraction / 0.02, 0.1, 5.0);
            return kEarthGreenhouseKelvin *
                   std::log(pressureKPa / kEarthPressureKPa + 1.0) *
                   std::sqrt(volatileScale);
        }

        // Temperature lapse rate for altitude: ~6.5 K / 1000m on Earth.
        // Scales with surface gravity.
        double AltitudeLapseKelvin(double elevationNormalized, double gravityMs2)
        {
            // Max "real" height of terrain: assume max normalized elevation ~1 maps to ~8 km
            constexpr double kMaxElevationMeters = 8000.0;
            constexpr double kEarthLapseRateKPerMeter = 0.0065;
            const double elevationMeters = std::max(0.0, elevationNormalized - 0.12) / 0.88 * kMaxElevationMeters;
            // Gravity affects the dry-adiabatic lapse rate
            const double lapseRate = kEarthLapseRateKPerMeter * (gravityMs2 / 9.81);
            return elevationMeters * lapseRate;
        }

        // Latitude-based Hadley + Ferrel + Polar circulation zones.
        // Returns expected precipitation factor [0..1] based on atmospheric circulation.
        // 0 = hyper-arid (subsidence), 1 = very wet (ITCZ or frontal zones).
        double CirculationPrecipitationFactor(double absLatDeg)
        {
            // ITCZ: 0-10° — wet
            // Subtropical highs: ~20-30° — dry
            // Mid-latitude westerlies: 40-60° — moderate
            // Polar fronts: ~60-70° — moderate-low
            // Polar cells: >70° — dry and cold

            // We model this as a sum of smooth "wet zones" in lat space
            const double latRad = absLatDeg * std::numbers::pi / 180.0;

            // ITCZ near equator (peak ~0°)
            const double itcz = std::exp(-latRad * latRad / (0.05));

            // Subtropical dry belt (centered ~25°)
            const double subtropicLatRad = 25.0 * std::numbers::pi / 180.0;
            const double subtropicDry = std::exp(
                -std::pow(latRad - subtropicLatRad, 2.0) / 0.04);

            // Mid-latitude frontal zone (centered ~55°)
            const double midLatLatRad = 55.0 * std::numbers::pi / 180.0;
            const double midLatWet = std::exp(
                -std::pow(latRad - midLatLatRad, 2.0) / 0.05) * 0.55;

            // Polar dry
            const double polarDry = std::max(0.0, (absLatDeg - 65.0) / 25.0);

            double result = itcz - subtropicDry * 0.65 + midLatWet - polarDry * 0.6;
            return std::clamp(result * 0.5 + 0.5, 0.0, 1.0);
        }

        // Ocean proximity moisture: regions near oceans receive more moisture.
        // This is a secondary noise signal scaled by the region's base moisture.
        double OceanMoistureFactor(double humidNoise, bool isOcean)
        {
            if (isOcean) return 1.0;
            return std::clamp(humidNoise * 0.8 + 0.3, 0.0, 1.0);
        }

        // Classify biome from temperature, precipitation, elevation, and ocean flag.
        Biome ClassifyBiome(
            double tempC,
            double precipMm,
            double elevNorm,
            bool ocean)
        {
            if (ocean)
            {
                // Deep vs shallow ocean
                return elevNorm < -0.5 ? Biome::DeepOcean : Biome::Ocean;
            }

            const double elevFraction = std::clamp((elevNorm - 0.12) / 0.88, 0.0, 1.0);

            // High alpine / glacier
            if (elevFraction > 0.85 || tempC < -20.0)
            {
                return tempC < -10.0 ? Biome::Glacier : Biome::AlpineRock;
            }

            // Polar / ice cap
            if (tempC < -15.0)
            {
                return Biome::IceCap;
            }

            // Tundra
            if (tempC < 0.0)
            {
                return Biome::Tundra;
            }

            // Boreal taiga
            if (tempC < 8.0)
            {
                return precipMm > 300.0 ? Biome::Boreal : Biome::Tundra;
            }

            // Cool-temperate
            if (tempC < 16.0)
            {
                if (precipMm > 600.0) return Biome::Temperate;
                if (precipMm > 280.0) return Biome::Grassland;
                return Biome::Shrubland;
            }

            // Warm-temperate to tropical
            if (tempC < 22.0)
            {
                if (precipMm > 800.0) return Biome::Temperate;
                if (precipMm > 400.0) return Biome::Grassland;
                if (precipMm > 200.0) return Biome::Shrubland;
                return Biome::Desert;
            }

            // Tropical
            if (precipMm > 1200.0) return Biome::TropicalRainforest;
            if (precipMm > 700.0)  return Biome::Savanna;
            if (precipMm > 300.0)  return Biome::Grassland;
            return Biome::Desert;
        }

    } // anonymous namespace

    // -----------------------------------------------------------------------
    // PlanetMap
    // -----------------------------------------------------------------------

    PlanetMap::PlanetMap(PlanetParameters parameters, ClimateInputs climate)
        : m_planet(std::move(parameters))
        , m_climate(climate)
    {
    }

    const Planet& PlanetMap::GetPlanet() const noexcept
    {
        return m_planet;
    }

    const PlanetParameters& PlanetMap::GetParameters() const noexcept
    {
        return m_planet.GetParameters();
    }

    const ClimateInputs& PlanetMap::GetClimateInputs() const noexcept
    {
        return m_climate;
    }

    int PlanetMap::GetLatitudeBandCount() const noexcept
    {
        return m_planet.GetParameters().latitudeBands;
    }

    int PlanetMap::GetLongitudeSegmentCount() const noexcept
    {
        return m_planet.GetParameters().longitudeSegments;
    }

    PlanetRegion PlanetMap::GetRegion(int latitudeIndex, int longitudeIndex) const
    {
        const int latitudeBands = std::max(1, GetLatitudeBandCount());
        const int longitudeSegments = std::max(1, GetLongitudeSegmentCount());

        const int normLat = std::clamp(latitudeIndex, 0, latitudeBands - 1);
        const int normLon = std::clamp(longitudeIndex, 0, longitudeSegments - 1);

        const auto regionCenter = m_planet.GetRegionCenter(normLat, normLon);
        const std::uint32_t regionSeed = m_planet.DeriveRegionSeed(normLat, normLon);
        const std::uint32_t planetSeed = m_planet.GetSeed();

        // -----------------------------------------------------------------------
        // Terrain elevation
        // -----------------------------------------------------------------------
        const double elevNoise = SmoothNoise2D(normLat, normLon, latitudeBands, longitudeSegments, planetSeed);
        const double elevationNormalized = ClampNormalized((elevNoise - 0.5) * 2.0);

        // Ocean threshold determined by volatile fraction: water-rich worlds have more ocean
        // volatileMassFraction=0.02 → ~30% land (threshold ~0.12 in [-1,1])
        // More volatiles = more ocean (lower threshold)
        const double oceanThreshold = std::clamp(
            0.20 - (m_climate.volatileMassFraction / 0.02 - 1.0) * 0.10,
            -0.1, 0.40);
        const bool ocean = elevationNormalized < oceanThreshold;

        // -----------------------------------------------------------------------
        // Temperature — multi-factor model
        // -----------------------------------------------------------------------

        // 1. Global equilibrium baseline from stellar flux + greenhouse
        const double equilibriumTempK = EquilibriumTemperatureKelvin(m_climate.meanIncidentFluxInEarthUnits);
        const double greenhouse = GreenhouseWarming(m_climate.atmospherePressureKPa, m_climate.volatileMassFraction);

        // 2. Latitude-dependent insolation fraction
        const double insolFraction = LatitudeInsolationFraction(
            regionCenter.latitudeDegrees, m_climate.axialTiltDegrees);

        // The local equilibrium temperature at this latitude (before terrain)
        // We scale the global equilibrium: equator gets ~1.3x, poles ~0.0
        // The +greenhouse shifts the whole planet warmer
        const double localEquilK = (equilibriumTempK - 255.0) * (insolFraction * 1.6 + 0.05) + 255.0 + greenhouse;

        // 3. Ocean thermal damping — oceans moderate temperature extremes
        const double thermalBuffer = ocean ? 0.75 : 1.0;
        const double latScaledTempK = (localEquilK - 255.0) * thermalBuffer + 255.0;

        // 4. Altitude cooling
        const double lapseK = AltitudeLapseKelvin(elevationNormalized, m_climate.gravityMetersPerSecondSquared);

        // 5. Noise perturbation for local variability (±5°C)
        const double tempNoise = MoistureNoise2D(normLat, normLon, latitudeBands, longitudeSegments, planetSeed ^ 0xaa55cc77);
        const double tempNoisePerturbation = (tempNoise - 0.5) * 10.0;

        const double temperatureCelsius = latScaledTempK - 273.15 - lapseK + tempNoisePerturbation;

        // -----------------------------------------------------------------------
        // Precipitation & humidity — atmospheric circulation model
        // -----------------------------------------------------------------------

        // Base moisture availability from volatiles
        const double moistureBase = std::clamp(m_climate.volatileMassFraction / 0.02, 0.1, 4.0);

        // Atmospheric circulation contribution
        const double circFactor = CirculationPrecipitationFactor(std::abs(regionCenter.latitudeDegrees));

        // Local moisture noise (wind/orographic effects)
        const double moistNoise = MoistureNoise2D(normLat, normLon, latitudeBands, longitudeSegments, planetSeed ^ 0x1234abcd);

        // Ocean evaporation: oceans are source of moisture, coasts are wetter
        const double oceanMoisture = OceanMoistureFactor(moistNoise, ocean);

        // Orographic effect: elevated terrain can force precipitation on windward side
        const double orographicBoost = ocean ? 0.0 :
            std::max(0.0, std::sin(elevationNormalized * std::numbers::pi * 2.0)) * 0.4;

        // Combined humidity
        const double humidityNormalized = std::clamp(
            circFactor * oceanMoisture * moistureBase * (1.0 + orographicBoost),
            0.0, 1.0);

        // Precipitation is a product of humidity, pressure (denser air carries more water),
        // and temperature (cold air holds less water — limits precip at poles)
        const double pressureScale = std::clamp(m_climate.atmospherePressureKPa / 101.325, 0.2, 3.0);
        const double warmingBoost = ocean ? 1.0 : std::clamp((temperatureCelsius + 10.0) / 30.0, 0.1, 2.0);
        const double precipitationMm = std::max(0.0,
            humidityNormalized * 1200.0 * pressureScale * warmingBoost);

        // -----------------------------------------------------------------------
        // Biome
        // -----------------------------------------------------------------------
        const Biome biome = ClassifyBiome(temperatureCelsius, precipitationMm, elevationNormalized, ocean);

        // -----------------------------------------------------------------------
        // Assemble region
        // -----------------------------------------------------------------------
        PlanetRegion region;
        region.latitudeIndex = normLat;
        region.longitudeIndex = normLon;
        region.seed = regionSeed;
        region.latitudeDegrees = regionCenter.latitudeDegrees;
        region.longitudeDegrees = regionCenter.longitudeDegrees;
        region.elevationNormalized = elevationNormalized;
        region.temperatureCelsius = temperatureCelsius;
        region.precipitationMm = precipitationMm;
        region.humidityNormalized = humidityNormalized;
        region.isOcean = ocean;
        region.biome = biome;
        return region;
    }

    double PlanetMap::SampleElevation(int latitudeIndex, int longitudeIndex) const
    {
        return GetRegion(latitudeIndex, longitudeIndex).elevationNormalized;
    }

    bool PlanetMap::IsOcean(int latitudeIndex, int longitudeIndex) const
    {
        return GetRegion(latitudeIndex, longitudeIndex).isOcean;
    }
}
