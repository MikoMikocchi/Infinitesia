// PlanetMap.cpp
// Multi-stage deterministic planet generation pipeline.
//
// Stages (executed once in constructor, results cached in m_grid):
//   1. Tectonics   — plate Voronoi, boundary classification, base crust
//   2. Terrain     — tectonic uplift + fBm noise + volcanic hotspots
//   3. Hydrology   — drainage accumulation, rivers, lakes
//   4. Climate     — stellar flux, latitude insolation, circulation zones,
//                    orographic effects, continentality
//   5. Soils       — parent rock, weathering, organic matter
//   6. Biomes      — Köppen-extended 23-type classification

#include "planet/PlanetMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

namespace Infinitesia
{

// ==========================================================================
// Internal helpers (anonymous namespace)
// ==========================================================================
namespace
{

// --------------------------------------------------------------------------
// Deterministic hashing / random numbers
// --------------------------------------------------------------------------

constexpr std::uint32_t kHashMul1 = 0x9e3779b9u;
constexpr std::uint32_t kHashMul2 = 0x85ebca6bu;

std::uint32_t Hash32(std::uint32_t v)
{
    v ^= v >> 16; v *= kHashMul2;
    v ^= v >> 13; v *= 0xc2b2ae3du;
    v ^= v >> 16;
    return v;
}

std::uint32_t HashCombine(std::uint32_t a, std::uint32_t b)
{
    return Hash32(a ^ (b * kHashMul1));
}

// Returns [0..1) from two coordinates + seed
double RandUnit(std::uint32_t seed, std::uint32_t tag)
{
    const std::uint32_t h = Hash32(seed ^ (tag * kHashMul1));
    return static_cast<double>(h) / static_cast<double>(0xffffffffu);
}

double RandUnit(std::uint32_t seed, int lat, int lon)
{
    return RandUnit(seed,
        static_cast<std::uint32_t>(lat) * kHashMul1 ^
        static_cast<std::uint32_t>(lon) * kHashMul2);
}

double RandRange(std::uint32_t seed, std::uint32_t tag, double lo, double hi)
{
    return lo + (hi - lo) * RandUnit(seed, tag);
}

// --------------------------------------------------------------------------
// Interpolation
// --------------------------------------------------------------------------

double SmootherStep(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

double Lerp(double a, double b, double t)
{
    return a + (b - a) * std::clamp(t, 0.0, 1.0);
}

// --------------------------------------------------------------------------
// 2-D value noise (longitude wraps, latitude clamps)
// --------------------------------------------------------------------------
double ValueNoise2D(double nLat, double nLon, int latCells, int lonCells, std::uint32_t seed)
{
    const double lp = std::clamp(nLat, 0.0, 1.0) * latCells;
    const double lo = (nLon - std::floor(nLon)) * lonCells;
    const int la0 = std::min(static_cast<int>(lp), latCells);
    const int la1 = std::min(la0 + 1, latCells);
    const int lo0 = static_cast<int>(lo) % lonCells;
    const int lo1 = (lo0 + 1) % lonCells;
    const double bt = SmootherStep(lp - la0);
    const double bl = SmootherStep(lo - std::floor(lo));

    const auto s = [seed, lonCells](int la, int lo_) -> double {
        const std::uint32_t v = seed ^
            (static_cast<std::uint32_t>(la + 1) * kHashMul1) ^
            (static_cast<std::uint32_t>(lo_ + 1) * kHashMul2);
        return static_cast<double>(Hash32(v)) / static_cast<double>(0xffffffffu);
    };

    const double n  = s(la0, lo0) + (s(la0, lo1) - s(la0, lo0)) * bl;
    const double ss = s(la1, lo0) + (s(la1, lo1) - s(la1, lo0)) * bl;
    return n + (ss - n) * bt;
}

// fBm over value noise
double fBm(double nLat, double nLon, std::uint32_t seed, int baseLat, int baseLon, int octaves = 6)
{
    double v = 0.0, amp = 1.0, total = 0.0;
    std::uint32_t s = seed;
    int lc = baseLat, lnc = baseLon;
    for (int o = 0; o < octaves; ++o)
    {
        v += ValueNoise2D(nLat, nLon, lc, lnc, s) * amp;
        total += amp;
        amp *= 0.5;
        lc *= 2; lnc *= 2;
        s = Hash32(s ^ 0xdeadbeef);
    }
    return v / total;
}

// fBm grid-index variant
double fBmGrid(int la, int lo, int latB, int lonS, std::uint32_t seed,
               int baseLat = 6, int baseLon = 12, int octaves = 6)
{
    const double nLat = (la + 0.5) / latB;
    const double nLon = (lo + 0.5) / lonS;
    return fBm(nLat, nLon, seed, baseLat, baseLon, octaves);
}

// --------------------------------------------------------------------------
// Spherical distance helpers
// --------------------------------------------------------------------------
struct Vec2d { double x = 0.0, y = 0.0; };
struct Vec3d { double x = 0.0, y = 0.0, z = 0.0; };

Vec3d LatLonToCart(double latDeg, double lonDeg)
{
    const double lat = latDeg * std::numbers::pi / 180.0;
    const double lon = lonDeg * std::numbers::pi / 180.0;
    return { std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
}

// Great-circle angular distance in degrees
double GCDistance(double la1, double lo1, double la2, double lo2)
{
    const Vec3d a = LatLonToCart(la1, lo1);
    const Vec3d b = LatLonToCart(la2, lo2);
    const double dot = std::clamp(a.x*b.x + a.y*b.y + a.z*b.z, -1.0, 1.0);
    return std::acos(dot) * 180.0 / std::numbers::pi;
}

// --------------------------------------------------------------------------
// Tectonic plate descriptor — with physical velocity vectors
// --------------------------------------------------------------------------

// Plate velocity in degrees per million years on the lat/lon sphere surface.
// Earth plates: 1–10 cm/yr ≈ 1–10 km/Myr ≈ 0.009–0.09 deg/Myr
// We work in normalised My so 1 unit = typical plate lifetime.
struct PlateVelocity
{
    double vLat = 0.0;  // deg/My, positive = northward
    double vLon = 0.0;  // deg/My, positive = eastward
};

struct TectonicPlate
{
    int    id          = 0;
    double centerLat   = 0.0;
    double centerLon   = 0.0;
    bool   isOceanic   = false;

    // Density and thickness
    double crustDensity       = 2900.0; // kg/m³  (oceanic ~2900, continental ~2750)
    double crustThicknessKm   = 7.0;    // km     (oceanic 5–10, continental 25–70)

    // Kinematics — derived from simulated mantle convection
    PlateVelocity velocity{};
};

// Classify boundary type from relative velocity and boundary normal.
// normal_ab: unit vector pointing from plate A toward plate B at the boundary
//            in 2D lat/lon tangent space.
enum class BoundaryType { Divergent, Convergent, Transform };

BoundaryType ClassifyBoundaryFromVelocity(
    const PlateVelocity& va,
    const PlateVelocity& vb,
    double normalLat,   // lat-component of boundary normal (A→B direction)
    double normalLon)   // lon-component of boundary normal (A→B direction)
{
    // Relative velocity of A with respect to B
    const double dvLat = va.vLat - vb.vLat;
    const double dvLon = va.vLon - vb.vLon;

    // Component of relative motion along the boundary normal
    // Positive = A moves away from B = Divergent
    // Negative = A moves toward B   = Convergent
    const double normalComponent = dvLat * normalLat + dvLon * normalLon;

    // Magnitude of tangential (strike-slip) component
    const double tangential = std::sqrt(
        std::max(0.0,
            dvLat * dvLat + dvLon * dvLon - normalComponent * normalComponent));

    constexpr double kConvergentThreshold =  0.3; // deg/My
    constexpr double kDivergentThreshold  = -0.3; // deg/My

    // Classify by which component dominates
    if (normalComponent < kDivergentThreshold)
        return BoundaryType::Convergent;
    if (normalComponent > -kDivergentThreshold) // note: same magnitude, opposite sign for diverge
        return BoundaryType::Divergent;
    return BoundaryType::Transform;
}

// Boundary subtype after velocity classification + plate types
enum class BoundarySubtype
{
    MidOceanRidge,          // Divergent,  oceanic–oceanic
    ContinentalRift,        // Divergent,  continental–continental or mixed
    OceanicSubduction,      // Convergent, oceanic–oceanic → island arc
    ContinentalSubduction,  // Convergent, oceanic under continental → volcanic arc + trench
    ContinentalCollision,   // Convergent, continental–continental → orogen
    TransformFault,         // Transform, any combination
};

BoundarySubtype DetailedBoundary(
    BoundaryType bt, bool aOceanic, bool bOceanic)
{
    switch (bt)
    {
    case BoundaryType::Divergent:
        return (aOceanic && bOceanic)
            ? BoundarySubtype::MidOceanRidge
            : BoundarySubtype::ContinentalRift;

    case BoundaryType::Convergent:
        if (!aOceanic && !bOceanic)    return BoundarySubtype::ContinentalCollision;
        if (aOceanic  && bOceanic)     return BoundarySubtype::OceanicSubduction;
        return BoundarySubtype::ContinentalSubduction; // one oceanic, one continental

    case BoundaryType::Transform:
        return BoundarySubtype::TransformFault;
    }
    return BoundarySubtype::TransformFault;
}

// --------------------------------------------------------------------------
// Airy Isostasy — compute surface elevation [km] from crust thickness
// --------------------------------------------------------------------------
//
// Airy isostasy: a crustal column floats on the mantle like ice on water.
// The deeper the "root", the higher the mountain.
//
//   h = T * (ρ_m - ρ_c) / ρ_m  - C
//
// where C is a reference offset so that "normal" ocean floor ≈ −3.5 km.
//
// Parameters from Earth:
//   ρ_mantle  ≈ 3300 kg/m³
//   ρ_oceanic ≈ 2900 kg/m³  → T=7 km    → h ≈ 0.85 km above mantle, −3.5 km below sea
//   ρ_cont    ≈ 2750 kg/m³  → T=35 km   → h ≈ 5.8 km above mantle  ≈ +0.5 km elevation
//   collision → T=70 km   → h ≈ 11.5 km above mantle → ~+5.5 km after offset (Himalaya)
//
constexpr double kMantle    = 3300.0; // kg/m³
constexpr double kOceanicRho  = 2900.0;
constexpr double kContinentalRho = 2750.0;
// Reference offset: sets the "zero" so that normal ocean floor ≈ −3.5 km
// Normal oceanic crust: T=7, ρ=2900 → column = 7*(3300−2900)/3300 = 0.848 km
// We want that to sit at −3.5 km → offset = 0.848 − (−3.5) = 4.348
constexpr double kIsostasyOffset = 4.35; // km, subtract this to get km above sea level

double AiryIsostasyKm(double crustThicknessKm, double crustDensity)
{
    const double column = crustThicknessKm * (kMantle - crustDensity) / kMantle;
    return column - kIsostasyOffset;
}

// Bathymetric cooling: oceanic crust subsides as it ages after spreading.
// depth(age) = depth_ridge + k * sqrt(age_Ma)
// Earth: ridge ≈ −2.6 km, k ≈ 0.35 km/sqrt(Ma)
double OceanicBathymetryKm(double crustAgeNorm, double maxAgeMy = 180.0)
{
    const double ageMy = crustAgeNorm * maxAgeMy;
    return -2.6 - 0.35 * std::sqrt(ageMy);
}

// --------------------------------------------------------------------------
// Climate helpers
// --------------------------------------------------------------------------

// Annual-mean insolation fraction vs latitude + axial tilt
double InsolationFraction(double latDeg, double tiltDeg)
{
    const double lat  = std::abs(latDeg) * std::numbers::pi / 180.0;
    const double tilt = tiltDeg * std::numbers::pi / 180.0;
    const double sinT = std::sin(tilt);
    const double cosL = std::max(0.0, std::cos(lat));
    // Tilt redistributes energy toward poles in annual mean
    const double sinL = std::sin(lat);
    const double tiltBoost = std::max(0.0, 1.0 - sinL*sinL*(1.0 - sinT*sinT));
    const double insol = (1.0 - sinT)*cosL + sinT*std::sqrt(tiltBoost)*0.5;
    return std::clamp(insol, 0.0, 1.0);
}

// Seasonal amplitude (difference summer–winter at this latitude)
double SeasonalAmplitude(double latDeg, double tiltDeg, double rotHours)
{
    const double lat  = std::abs(latDeg) * std::numbers::pi / 180.0;
    const double tilt = tiltDeg * std::numbers::pi / 180.0;
    // Stronger seasons at higher latitudes and higher tilt
    const double baseAmplitude = std::sin(lat) * std::sin(tilt) * 50.0;
    // Fast rotators have stronger day/night cycling but shorter seasons — not a big change here
    // Slow rotators (Venus-like) reduce seasonal contrast slightly
    const double rotFactor = std::clamp(std::log(rotHours / 24.0 + 1.0) / 1.0, 0.5, 1.5);
    return std::clamp(baseAmplitude * rotFactor, 0.0, 60.0);
}

// Equilibrium T from stellar flux
double EquilibriumTempK(double flux)
{
    return 288.0 * std::pow(std::max(0.01, flux), 0.25);
}

// Greenhouse effect from pressure and volatiles
double GreenhouseK(double pressKPa, double volFrac)
{
    if (pressKPa <= 0.0) return 0.0;
    const double vScale = std::clamp(volFrac / 0.02, 0.1, 5.0);
    return 30.0 * std::log(pressKPa / 101.325 + 1.0) * std::sqrt(vScale);
}

// Altitude lapse cooling [K].
// elevNorm in [-1..1] where 1.0 = +9 km.
// Sea level = 0.0, so only positive elevation contributes.
double LapseK(double elevNorm, double gravMs2)
{
    const double elevM = std::max(0.0, elevNorm) * 9000.0; // metres above sea level
    const double lapse = 0.0065 * (gravMs2 / 9.81);        // gravity-adjusted lapse rate
    return elevM * lapse;
}

// Zonal circulation — returns [0..1] wetness from Hadley/Ferrel/Polar cells
// Modulated by rotation period: fast rotation → tighter cells, stronger drying belts
double CirculationWetness(double absLatDeg, double rotHours)
{
    // Earth reference cells: ITCZ ~0°, subtropical dry ~25°, mid-lat front ~55°, polar ~80°
    // Faster rotation → cells compress toward equator
    const double cellScale = std::clamp(24.0 / rotHours, 0.4, 2.0);

    const double lat = absLatDeg * std::numbers::pi / 180.0;

    // ITCZ (ascending moist air, ~0–12°)
    const double itczCenter = 0.0;
    const double itczWidth  = 0.12 * cellScale;
    const double itcz = std::exp(-std::pow(lat - itczCenter, 2.0) / itczWidth);

    // Subtropical dry belt (~25°, descending dry air)
    const double subC = 25.0 / cellScale * std::numbers::pi / 180.0;
    const double subW = 0.04;
    const double sub  = std::exp(-std::pow(lat - subC, 2.0) / subW);

    // Mid-latitude frontal zone (~55°)
    const double midC = 55.0 / cellScale * std::numbers::pi / 180.0;
    const double midW = 0.05;
    const double mid  = 0.60 * std::exp(-std::pow(lat - midC, 2.0) / midW);

    // Polar dry (>70°)
    const double polarDry = std::max(0.0, (absLatDeg - 65.0) / 25.0);

    const double raw = itcz - sub * 0.75 + mid - polarDry * 0.5;
    return std::clamp(raw * 0.5 + 0.5, 0.0, 1.0);
}

// Dry-season months from precipitation distribution curve
double DrySeasonMonths(double precipMm, double circulationWetness)
{
    // Tropical convergence → rain year-round, no dry season
    // Subtropical → strong dry season
    // The subtropical dip in circulation gives a dry-season signal
    const double dryFraction = std::clamp(1.0 - circulationWetness * 1.4, 0.0, 1.0);
    return dryFraction * 7.0; // 0..7 months
}

// Soil fertility from rock type, climate age, temperature, precip
double SoilFertility(RockType rock, double tempC, double precipMm, double planetAgeFrac)
{
    // Base fertility from rock
    double base = 0.2;
    switch (rock) {
        case RockType::VolcanicIgneous:    base = 0.65; break; // volcanics weather fast, very fertile
        case RockType::BasalticOceanic:    base = 0.35; break;
        case RockType::SedimentaryBasin:   base = 0.55; break;
        case RockType::FoldedSedimentary:  base = 0.45; break;
        case RockType::GraniticShield:     base = 0.30; break;
        case RockType::MetamorphicBelt:    base = 0.25; break;
    }
    // Weathering and humus accumulation: peaks at moderate warm+wet
    const double tempFactor = std::clamp((tempC + 5.0) / 30.0, 0.0, 1.0);
    const double wetFactor  = std::clamp(precipMm / 900.0, 0.0, 1.0);
    // Cold or hyper-arid → thin soils
    const double weathering = tempFactor * wetFactor;
    // Old planet → more developed soils (but leached in tropics over time)
    const double ageFactor = std::clamp(planetAgeFrac, 0.0, 1.0);
    const double leaching  = (tempC > 22.0 && precipMm > 1500.0) ? 0.6 : 1.0;
    return std::clamp(base * (0.3 + 0.7 * weathering) * (0.5 + 0.5 * ageFactor) * leaching, 0.0, 1.0);
}

// Soil depth from rock type and erosion proxy
double SoilDepth(RockType rock, double precipMm, double slope, double planetAgeFrac)
{
    double base = 0.3;
    switch (rock) {
        case RockType::SedimentaryBasin:  base = 0.8; break;
        case RockType::VolcanicIgneous:   base = 0.5; break;
        case RockType::BasalticOceanic:   base = 0.3; break;
        case RockType::GraniticShield:    base = 0.4; break;
        case RockType::FoldedSedimentary: base = 0.5; break;
        case RockType::MetamorphicBelt:   base = 0.25; break;
    }
    // Rain helps weather rock → deeper soils; slope removes soil
    const double wetFactor   = std::clamp(precipMm / 1200.0, 0.0, 1.0);
    const double slopeFactor = 1.0 - std::clamp(slope * 1.5, 0.0, 0.9);
    const double ageFactor   = std::clamp(planetAgeFrac, 0.1, 1.0);
    return std::clamp(base * wetFactor * slopeFactor * ageFactor, 0.0, 1.0);
}

// --------------------------------------------------------------------------
// Biome classification — full Köppen-extended
// elevation is now in [-1..1] where 1.0 = +9 km above sea level
// Alpine starts at ~2500 m = 2.5/9.0 ≈ 0.28 normalised
// --------------------------------------------------------------------------
Biome ClassifyBiome(const PlanetRegion& r)
{
    // elevationKm: physical elevation in km (positive = above sea level)
    const double elevKm   = r.elevationNormalized * 9.0;
    const double t        = r.temperatureCelsius;
    const double p        = r.precipitationMm;
    const double dry      = r.drySeasonMonths;
    const double snow     = r.snowDays;

    // --- Aquatic ---
    if (r.isOcean)
    {
        if (t < -2.0)       return Biome::PolarSea;
        if (elevKm < -4.5)  return Biome::DeepOcean;
        if (t > 20.0 && elevKm > -0.4) return Biome::CoralReef;
        return Biome::ShallowSea;
    }

    // Lake
    if (r.isLake) return Biome::ShallowSea;

    // --- Salt flat: hot endorheic basin, near-zero precip ---
    if (t > 15.0 && p < 80.0 && r.drainageAccumulation < 0.01)
        return Biome::SaltFlat;

    // --- Volcanic terrain ---
    if (r.isVolcanicZone && elevKm < 3.0 && t > -5.0 && p < 250.0)
        return Biome::VolcanicTerrain;

    // --- Alpine (> ~2500 m) ---
    if (elevKm > 4.5)
        return (t < -5.0) ? Biome::Glacier : Biome::AlpineRock;
    if (elevKm > 2.5 && t < 6.0)
        return (snow > 150.0) ? Biome::Glacier : Biome::AlpineGrassland;

    // --- Polar / ice ---
    if (t < -20.0) return Biome::IceCap;
    if (t < -15.0) return Biome::Glacier;

    // --- Tundra ---
    if (t < -2.0)
        return (p > 350.0) ? Biome::WetTundra : Biome::Tundra;

    // --- Boreal taiga ---
    if (t < 7.0)
    {
        if (p < 250.0) return Biome::Tundra;
        if (r.soilMoisture > 0.7) return Biome::BorealWetland;
        return Biome::BorealForest;
    }

    // --- Cold desert / steppe ---
    if (t < 14.0 && p < 200.0) return Biome::ColdDesert;
    if (t < 14.0 && p < 450.0) return Biome::TemperateSteppe;

    // --- Temperate ---
    if (t < 18.0)
    {
        if (dry > 3.0 && p > 300.0 && p < 900.0) return Biome::MediterraneanShrub;
        if (p > 700.0) return Biome::TemperateDeciduous;
        if (p > 400.0) return Biome::TemperateMixed;
        if (p > 200.0) return Biome::TemperateSteppe;
        return Biome::ColdDesert;
    }

    // --- Hot desert ---
    if (p < 200.0) return Biome::HotDesert;

    // --- Subtropical / tropical ---
    if (dry > 4.0)
    {
        if (p > 800.0) return Biome::TropicalDryForest;
        if (p > 400.0) return Biome::Savanna;
        return Biome::HotDesert;
    }

    if (p > 1600.0) return Biome::TropicalRainforest;
    if (p > 900.0)  return Biome::TropicalDryForest;
    if (p > 500.0)  return Biome::Savanna;
    return Biome::HotDesert;
}

} // anonymous namespace

// ==========================================================================
// PlanetMap — construction (runs all stages)
// ==========================================================================

PlanetMap::PlanetMap(PlanetParameters parameters, ClimateInputs climate)
    : m_planet(std::move(parameters))
    , m_climate(climate)
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    m_grid.resize(static_cast<std::size_t>(latB) * static_cast<std::size_t>(lonS));

    // Pre-fill coordinates in every cell
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const auto ctr = m_planet.GetRegionCenter(la, lo);
            c.latitudeIndex  = la;
            c.longitudeIndex = lo;
            c.seed           = m_planet.DeriveRegionSeed(la, lo);
            c.latitudeDegrees  = ctr.latitudeDegrees;
            c.longitudeDegrees = ctr.longitudeDegrees;
        }
    }

    GenerateTectonics();
    GenerateTerrain();
    GenerateHydrology();
    GenerateClimate();
    GenerateSoils();
    ClassifyBiomes();
}

// --------------------------------------------------------------------------
// Accessors
// --------------------------------------------------------------------------

const Planet& PlanetMap::GetPlanet() const noexcept { return m_planet; }
const PlanetParameters& PlanetMap::GetParameters() const noexcept { return m_planet.GetParameters(); }
const ClimateInputs& PlanetMap::GetClimateInputs() const noexcept { return m_climate; }
int PlanetMap::GetLatitudeBandCount()    const noexcept { return m_planet.GetParameters().latitudeBands; }
int PlanetMap::GetLongitudeSegmentCount() const noexcept { return m_planet.GetParameters().longitudeSegments; }

const PlanetRegion& PlanetMap::GetRegion(int la, int lo) const noexcept
{
    return Cell(
        std::clamp(la, 0, LatBands() - 1),
        std::clamp(lo, 0, LonSegs() - 1));
}

double PlanetMap::SampleElevation(int la, int lo) const { return GetRegion(la, lo).elevationNormalized; }
bool   PlanetMap::IsOcean(int la, int lo) const         { return GetRegion(la, lo).isOcean; }

PlanetRegion& PlanetMap::Cell(int la, int lo)
{
    return m_grid[static_cast<std::size_t>(la) * static_cast<std::size_t>(LonSegs()) + lo];
}
const PlanetRegion& PlanetMap::Cell(int la, int lo) const
{
    return m_grid[static_cast<std::size_t>(la) * static_cast<std::size_t>(LonSegs()) + lo];
}

int PlanetMap::LatBands()  const noexcept { return m_planet.GetParameters().latitudeBands;    }
int PlanetMap::LonSegs()   const noexcept { return m_planet.GetParameters().longitudeSegments; }

// ==========================================================================
// Stage 1 — Tectonics: physically-derived plates, kinematics, boundaries
// ==========================================================================
void PlanetMap::GenerateTectonics()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    const std::uint32_t seed = m_planet.GetSeed();

    // -----------------------------------------------------------------------
    // Number of plates from internal heat production.
    // More massive planet → more radioactive heating → more convection cells.
    // Earth (~6e24 kg) → 12–15 major plates.
    // -----------------------------------------------------------------------
    const double massFrac   = m_planet.GetParameters().massKilograms / 5.972e24;
    const double ironFrac   = m_planet.GetParameters().bulkIronMassFraction;
    const double ageFrac    = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);
    // Older planets lose heat → fewer, larger plates (fewer active rifts)
    const double heatScale  = std::sqrt(massFrac) * (1.0 - 0.4 * ageFrac);
    // 5–16 plates
    const int numPlates = static_cast<int>(std::clamp(5.0 + heatScale * 11.0, 5.0, 16.0));

    // -----------------------------------------------------------------------
    // Continental fraction: volatile-rich worlds have more ocean.
    // ironFrac drives geochemical differentiation → continental crust.
    // -----------------------------------------------------------------------
    const double continentalFraction = std::clamp(
        ironFrac * 2.2 - (m_climate.volatileMassFraction / 0.02 - 1.0) * 0.1,
        0.25, 0.75);

    // -----------------------------------------------------------------------
    // Place plate centres: Fibonacci spiral for uniform distribution on sphere
    // -----------------------------------------------------------------------
    std::vector<TectonicPlate> plates(static_cast<std::size_t>(numPlates));
    const double goldenAngle = std::numbers::pi * (3.0 - std::sqrt(5.0));

    for (int p = 0; p < numPlates; ++p)
    {
        const auto pu = static_cast<std::uint32_t>(p);
        const double y     = 1.0 - (p / (numPlates - 1.0)) * 2.0;
        const double theta = goldenAngle * p +
                             RandRange(seed, pu * 13u + 1u, -0.25, 0.25);
        plates[p].id        = p;
        plates[p].centerLat = std::asin(std::clamp(y, -1.0, 1.0)) * 180.0 / std::numbers::pi;
        plates[p].centerLon = std::fmod(theta * 180.0 / std::numbers::pi + 360.0, 360.0);

        // Whether this plate carries mainly continental or oceanic crust
        plates[p].isOceanic = RandUnit(seed, pu * 7u + 1u) > continentalFraction;

        if (plates[p].isOceanic)
        {
            plates[p].crustDensity     = 2870.0 + RandRange(seed, pu*7u+3u, -40.0, 80.0);
            plates[p].crustThicknessKm = RandRange(seed, pu*7u+4u, 5.0, 10.0);
        }
        else
        {
            // Continental: thickness varies a lot (old cratons thick, rifts thin)
            plates[p].crustDensity     = 2720.0 + RandRange(seed, pu*7u+3u, -40.0, 60.0);
            plates[p].crustThicknessKm = RandRange(seed, pu*7u+4u, 28.0, 50.0);
        }

        // -----------------------------------------------------------------------
        // Velocity: driven by mantle convection (modelled as random with physical limits).
        // Real plates: 1–10 cm/yr ≈ 0.01–0.09 deg/Myr at Earth's equator.
        // We use normalised units: max speed ~ 0.09 deg/Myr → scaled to planet mass/age.
        // -----------------------------------------------------------------------
        const double speedScale = std::clamp(heatScale, 0.3, 2.5);
        const double speed = RandRange(seed, pu*7u+5u, 0.01, 0.09) * speedScale;
        const double angle = RandRange(seed, pu*7u+6u, 0.0, 2.0 * std::numbers::pi);
        plates[p].velocity.vLat = speed * std::cos(angle);
        plates[p].velocity.vLon = speed * std::sin(angle);
    }

    // -----------------------------------------------------------------------
    // Voronoi assignment: each cell → nearest plate centre (great-circle)
    // -----------------------------------------------------------------------
    // Also record second-nearest for boundary detection and normal computation.
    struct CellAssignment { int best = 0; int second = 0; double bestDist = 1e18; double secondDist = 1e18; };
    std::vector<CellAssignment> assignments(static_cast<std::size_t>(latB) * lonS);

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const auto& c = Cell(la, lo);
            auto& a = assignments[static_cast<std::size_t>(la) * lonS + lo];
            for (int p = 0; p < numPlates; ++p)
            {
                const double d = GCDistance(c.latitudeDegrees, c.longitudeDegrees,
                                            plates[p].centerLat, plates[p].centerLon);
                if (d < a.bestDist)
                {
                    a.secondDist  = a.bestDist;  a.second = a.best;
                    a.bestDist    = d;            a.best   = p;
                }
                else if (d < a.secondDist)
                {
                    a.secondDist  = d;            a.second = p;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Assign plate data and classify boundaries
    // -----------------------------------------------------------------------
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const auto& a = assignments[static_cast<std::size_t>(la) * lonS + lo];
            const TectonicPlate& plateA = plates[a.best];
            const TectonicPlate& plateB = plates[a.second];

            c.tectonicPlateId = a.best;

            // ----------------------------------------------------------------
            // Boundary proximity (0 = plate interior, 1 = on the boundary)
            // Smooth falloff over ~4 degrees arc
            // ----------------------------------------------------------------
            const double boundaryWidth = 4.0; // degrees
            c.tectonicStress = SmootherStep(
                std::clamp(1.0 - (a.secondDist - a.bestDist) / boundaryWidth, 0.0, 1.0));

            // ----------------------------------------------------------------
            // Boundary normal in 2D lat/lon space (direction from A centre → B centre,
            // projected onto the local tangent plane at the cell).
            // This is approximate but good enough for classifying convergent/divergent.
            // ----------------------------------------------------------------
            const double dLat = plateB.centerLat - plateA.centerLat;
            const double dLon = plateB.centerLon - plateA.centerLon;
            const double dLen = std::sqrt(dLat * dLat + dLon * dLon);
            const double normalLat = dLen > 0.0 ? dLat / dLen : 0.0;
            const double normalLon = dLen > 0.0 ? dLon / dLen : 1.0;

            // ----------------------------------------------------------------
            // Classify boundary from relative velocity (physical, not random)
            // ----------------------------------------------------------------
            const BoundaryType bt = ClassifyBoundaryFromVelocity(
                plateA.velocity, plateB.velocity, normalLat, normalLon);
            const BoundarySubtype bst = DetailedBoundary(bt, plateA.isOceanic, plateB.isOceanic);

            // ----------------------------------------------------------------
            // Interior crust properties (Voronoi-interior logic)
            // ----------------------------------------------------------------
            c.crustThickness = plateA.crustThicknessKm;
            c.crustAge = RandRange(seed,
                static_cast<std::uint32_t>(la * lonS + lo) ^ 0xbeef0000u,
                0.0, 1.0);

            if (plateA.isOceanic)
            {
                // Oceanic crust age increases with distance from nearest spreading ridge.
                // We approximate: distance from A centre (where MOR/rift would be)
                // is proportional to age.
                c.crustAge = std::clamp(a.bestDist / 90.0, 0.0, 1.0); // 0=new, 1=old
                c.surfaceRock = RockType::BasalticOceanic;
                c.isVolcanicZone = false;
            }
            else
            {
                // Continental interior
                if (c.crustAge > 0.75)
                    c.surfaceRock = RockType::GraniticShield;      // old craton
                else
                    c.surfaceRock = RockType::SedimentaryBasin;    // younger basin
                c.isVolcanicZone = false;
            }

            // ----------------------------------------------------------------
            // Boundary-zone overrides — applied proportional to stress
            // ----------------------------------------------------------------
            if (c.tectonicStress > 0.25)
            {
                switch (bst)
                {
                case BoundarySubtype::ContinentalCollision:
                    // Collision: thicken crust significantly → mountains via isostasy
                    // Real Himalaya: normal 35 km + 35 km → ~70 km collision zone
                    {
                        const double relativeSpeed =
                            std::hypot(plateA.velocity.vLat - plateB.velocity.vLat,
                                       plateA.velocity.vLon - plateB.velocity.vLon);
                        const double thickeningFactor = 1.0 + c.tectonicStress * 0.9 * relativeSpeed / 0.1;
                        c.crustThickness = plateA.crustThicknessKm * std::clamp(thickeningFactor, 1.0, 2.2);
                        c.surfaceRock    = RockType::MetamorphicBelt;
                        c.isVolcanicZone = false;
                    }
                    break;

                case BoundarySubtype::ContinentalSubduction:
                    // Subduction arc: volcanic mountains on the continental side
                    // The oceanic plate dips under → compression + volcanism
                    c.crustThickness = plateA.isOceanic
                        ? 5.0  // the subducting slab is thin
                        : plateA.crustThicknessKm * (1.0 + c.tectonicStress * 0.35);
                    c.surfaceRock    = RockType::VolcanicIgneous;
                    c.isVolcanicZone = true;
                    break;

                case BoundarySubtype::OceanicSubduction:
                    // Island arc: volcanism above subducting oceanic slab
                    c.surfaceRock    = RockType::VolcanicIgneous;
                    c.isVolcanicZone = true;
                    c.crustThickness = 12.0 + c.tectonicStress * 8.0; // thickened island arc
                    break;

                case BoundarySubtype::MidOceanRidge:
                    // MOR: very thin, hot, young basalt just being created
                    c.crustThickness = 4.0 + (1.0 - c.tectonicStress) * 3.0;
                    c.crustAge       = 0.0; // brand new
                    c.surfaceRock    = RockType::BasalticOceanic;
                    c.isVolcanicZone = true; // hydrothermal vents, seafloor spreading
                    break;

                case BoundarySubtype::ContinentalRift:
                    // Rift: crust thins, basin forms, future ocean
                    c.crustThickness = plateA.crustThicknessKm * (1.0 - c.tectonicStress * 0.4);
                    c.surfaceRock    = RockType::BasalticOceanic;
                    c.isVolcanicZone = true;
                    break;

                case BoundarySubtype::TransformFault:
                    // No major vertical displacement, just lateral
                    c.surfaceRock    = plateA.isOceanic
                        ? RockType::BasalticOceanic
                        : RockType::FoldedSedimentary;
                    c.isVolcanicZone = false;
                    break;
                }
            }

            // Oceanic hotspot: random volcanic seamounts within oceanic plates
            // Hawaii-style: isolated, away from boundaries
            if (plateA.isOceanic && c.tectonicStress < 0.1 &&
                RandUnit(seed ^ 0xbabe, la, lo) < 0.006)
            {
                c.isVolcanicZone  = true;
                c.surfaceRock     = RockType::VolcanicIgneous;
                c.crustThickness  = 18.0; // thickened seamount / island
            }
        }
    }
}

// ==========================================================================
// Stage 2 — Terrain: elevation from Airy isostasy + bathymetric cooling
//           fBm adds only local detail (~15% of signal)
// ==========================================================================
void PlanetMap::GenerateTerrain()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    const std::uint32_t seed = m_planet.GetSeed();

    // Planet age affects erosion smoothing
    const double ageFrac = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);

    // Gravity scaling for atmospheric pressure / erosion intensity
    const double gravScale = m_climate.gravityMetersPerSecondSquared / 9.81;

    // -----------------------------------------------------------------------
    // Pass 1: compute raw elevation [km] from isostasy / bathymetry
    // -----------------------------------------------------------------------
    // Physical elevation range: −11 km (Mariana Trench) to +9 km (Himalaya)
    // We store this in elevationKm temporarily, then normalise.
    std::vector<double> elevKm(static_cast<std::size_t>(latB) * lonS);

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const auto& c = Cell(la, lo);
            const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;

            double elev = 0.0;

            if (c.surfaceRock == RockType::BasalticOceanic && !c.isVolcanicZone)
            {
                // Pure oceanic crust: depth from bathymetric cooling formula
                // crustThickness encodes typical oceanic thickness,
                // crustAge [0..1] represents 0–180 Ma
                elev = OceanicBathymetryKm(c.crustAge, 180.0);

                // MOR (very young crust, low age): ridge sits at −2.6 km
                // Deep trench near subduction: subtype already changed crustThickness
            }
            else
            {
                // Continental and modified oceanic (arcs, hotspots): use Airy
                elev = AiryIsostasyKm(c.crustThickness, c.surfaceRock == RockType::BasalticOceanic
                    ? kOceanicRho
                    : kContinentalRho);
            }

            // ---------------------------------------------------------------
            // Subduction trench: deepest feature on the planet
            // We look for cases where this cell is near a subduction boundary
            // (indicated by being on the oceanic side = BasalticOceanic + high stress)
            // ---------------------------------------------------------------
            if (c.surfaceRock == RockType::BasalticOceanic &&
                c.tectonicStress > 0.4 && !c.isVolcanicZone)
            {
                // Trench: pull down proportional to stress and convergence
                const double trenchDepth = -8.0 - c.tectonicStress * 3.0; // −8 to −11 km
                elev = Lerp(elev, trenchDepth, c.tectonicStress * 0.8);
            }

            // ---------------------------------------------------------------
            // Foreland basin: compression in front of a collision belt
            // creates a flexural depression (−0.5 to −1.5 km depression)
            // ---------------------------------------------------------------
            if (c.surfaceRock == RockType::SedimentaryBasin && c.tectonicStress > 0.2)
            {
                elev -= c.tectonicStress * 1.2;
            }

            // ---------------------------------------------------------------
            // fBm detail: local roughness from unresolved geology
            // Scale is 15% of the total physical range (~20 km span)
            // = ±1.5 km of noise, reasonable for unresolved faulting/folding
            // ---------------------------------------------------------------
            const double detail = fBmGrid(la, lo, latB, lonS, seed, 4, 8, 6);
            elev += (detail - 0.5) * 3.0; // ±1.5 km

            elevKm[idx] = elev;
        }
    }

    // -----------------------------------------------------------------------
    // Pass 2: erosion smoothing — old planets have softer relief
    // Erosion acts on mountains: rivers and glaciers flatten peaks over time.
    // We apply a weighted average, stronger at high elevations.
    // -----------------------------------------------------------------------
    const double erosionStrength = ageFrac * 0.45 * gravScale; // Earth at 4.5 Gy → 45%

    if (erosionStrength > 0.02)
    {
        std::vector<double> smoothed = elevKm;
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                const double e = elevKm[idx];
                if (e < 0.0) continue; // don't smooth ocean floor

                double sum = 0.0; int count = 0;
                for (int dla = -1; dla <= 1; ++dla)
                    for (int dlo = -1; dlo <= 1; ++dlo)
                    {
                        const int nla = std::clamp(la + dla, 0, latB - 1);
                        const int nlo = ((lo + dlo) + lonS) % lonS;
                        sum += elevKm[static_cast<std::size_t>(nla) * lonS + nlo];
                        ++count;
                    }
                const double avg = sum / count;
                // Stronger erosion at higher elevations (mountains erode faster)
                const double eFactor = erosionStrength * std::clamp(e / 5.0, 0.0, 1.0);
                smoothed[idx] = Lerp(e, avg, eFactor);
            }
        }
        elevKm = smoothed;
    }

    // -----------------------------------------------------------------------
    // Pass 3: normalise to [-1..1] using fixed physical range
    // elevationNormalized = elevKm / kMaxElevKm
    // so that 1.0 = kMaxElevKm above sea level, −1.0 = kMaxElevKm below.
    // This makes the value physically meaningful:
    //   0.02 = +170 m, 0.5 = +4250 m (Alpine), −0.5 = −4250 m (deep ocean)
    // -----------------------------------------------------------------------
    constexpr double kMaxElevKm = 9.0;  // normalisation range: −9 to +9 km

    // Sea level is at elevKm = 0.0 → normalised 0.0
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const double ek = elevKm[static_cast<std::size_t>(la) * lonS + lo];
            c.elevationNormalized = std::clamp(ek / kMaxElevKm, -1.0, 1.0);
            c.isOcean = (ek < 0.0);
        }
    }

    // -----------------------------------------------------------------------
    // Pass 4: slope (gradient magnitude) for orography and soil calculations
    // -----------------------------------------------------------------------
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const int la1 = std::clamp(la - 1, 0, latB - 1);
            const int la2 = std::clamp(la + 1, 0, latB - 1);
            const int lo1 = ((lo - 1) + lonS) % lonS;
            const int lo2 = ((lo + 1) + lonS) % lonS;
            // dElev in km per grid cell (~100 km) → slope in km/km
            const double dkm = kMaxElevKm * 2.0; // denominator for the full normalised range
            const double dLat = (Cell(la2, lo).elevationNormalized - Cell(la1, lo).elevationNormalized) * dkm;
            const double dLon = (Cell(la, lo2).elevationNormalized - Cell(la, lo1).elevationNormalized) * dkm;
            // Divide by 2 (two-cell span) and approximate horizontal distance ~200 km
            c.slopeNormalized = std::clamp(std::hypot(dLat, dLon) / (2.0 * 200.0), 0.0, 1.0);
        }
    }
}

// ==========================================================================
// Stage 3 — Hydrology
// ==========================================================================
void PlanetMap::GenerateHydrology()
{
    const int latB = LatBands();
    const int lonS = LonSegs();

    // Drainage accumulation: for each land cell, find its downslope neighbor
    // and accumulate flow count.
    std::vector<int> accumulation(static_cast<std::size_t>(latB) * lonS, 0);

    // Flow direction: index of downslope neighbor (or -1 for ocean/sink)
    std::vector<int> flowDir(static_cast<std::size_t>(latB) * lonS, -1);

    const int dx[8] = { -1, -1, -1,  0,  0,  1,  1,  1 };
    const int dy[8] = { -1,  0,  1, -1,  1, -1,  0,  1 };

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const auto& c = Cell(la, lo);
            if (c.isOcean) continue;

            double minElev = c.elevationNormalized;
            int   bestNeighbor = -1;
            for (int d = 0; d < 8; ++d)
            {
                const int nla = la + dx[d];
                const int nlo = ((lo + dy[d]) + lonS) % lonS;
                if (nla < 0 || nla >= latB) continue;
                const double ne = Cell(nla, nlo).elevationNormalized;
                if (ne < minElev) { minElev = ne; bestNeighbor = nla * lonS + nlo; }
            }
            flowDir[static_cast<std::size_t>(la)*lonS + lo] = bestNeighbor;
        }
    }

    // Process cells in order of elevation (lowest first) for correct accumulation
    std::vector<int> order(static_cast<std::size_t>(latB) * lonS);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return m_grid[a].elevationNormalized < m_grid[b].elevationNormalized;
    });

    for (int idx : order)
    {
        const int la = idx / lonS;
        const int lo = idx % lonS;
        if (Cell(la, lo).isOcean) continue;
        accumulation[idx] += 1;
        const int target = flowDir[idx];
        if (target >= 0)
            accumulation[target] += accumulation[idx];
    }

    // Find max accumulation for normalisation
    const int maxAcc = *std::max_element(accumulation.begin(), accumulation.end());

    // Assign river and lake data
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const int acc = accumulation[static_cast<std::size_t>(la)*lonS + lo];
            c.drainageAccumulation = maxAcc > 0
                ? static_cast<double>(acc) / maxAcc
                : 0.0;

            // River: significant accumulated flow on land
            const double riverThreshold = 0.03;
            c.hasRiver      = !c.isOcean && c.drainageAccumulation > riverThreshold;
            c.riverDischarge = c.hasRiver ? c.drainageAccumulation : 0.0;

            // Endorheic lake: land cell with no downslope neighbour (flowDir == -1)
            // and enough catchment
            c.isLake = !c.isOcean &&
                       flowDir[static_cast<std::size_t>(la)*lonS + lo] == -1 &&
                       c.drainageAccumulation > 0.005;
        }
    }
}

// ==========================================================================
// Stage 4 — Climate
// ==========================================================================
void PlanetMap::GenerateClimate()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    const std::uint32_t seed = m_planet.GetSeed();

    const double flux     = m_climate.meanIncidentFluxInEarthUnits;
    const double tilt     = m_climate.axialTiltDegrees;
    const double rot      = m_climate.rotationPeriodHours;
    const double pressKPa = m_climate.atmospherePressureKPa;
    const double volFrac  = m_climate.volatileMassFraction;
    const double grav     = m_climate.gravityMetersPerSecondSquared;

    const double globalEquilK = EquilibriumTempK(flux);
    const double greenhouse   = GreenhouseK(pressKPa, volFrac);
    const double pressureScale = std::clamp(pressKPa / 101.325, 0.1, 4.0);

    // Sea level is at elevKm = 0.0 → normalised 0.0 → no threshold needed
    constexpr double kOceanThreshold = 0.0; // kept for reference

    // Pass 1: temperature and gross humidity from latitude + elevation + pressure
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const double absLat = std::abs(c.latitudeDegrees);

            // ---- Temperature ----
            const double insolF = InsolationFraction(c.latitudeDegrees, tilt);
            // Scale: sub-solar point gets ~1.4× global equilibrium, poles near 0
            const double latTempK = (globalEquilK - 240.0) * (insolF * 1.5 + 0.04) + 240.0 + greenhouse;
            // Ocean thermal buffer (oceans don't swing as far)
            const double thermalBuf = c.isOcean ? 0.70 : 1.0;
            const double bufferedK  = (latTempK - 250.0) * thermalBuf + 250.0;
            // Altitude lapse
            const double lapseK_ = LapseK(c.elevationNormalized, grav);
            // Local noise ±4°C
            const double noisePert = (fBmGrid(la, lo, latB, lonS, seed ^ 0xaa55cc77, 5, 10, 4) - 0.5) * 8.0;

            c.temperatureCelsius = bufferedK - 273.15 - lapseK_ + noisePert;

            // ---- Seasonal amplitude ----
            // Ocean moderates seasons; land amplifies
            const double baseAmplitude = SeasonalAmplitude(c.latitudeDegrees, tilt, rot);
            c.temperatureAnnualRangeC = baseAmplitude * (c.isOcean ? 0.4 : 1.0);

            // ---- Continental index ----
            // Cells far from ocean get more continental character
            // Approximated by local land/ocean mix in a neighbourhood
            c.continentality = 0.0; // will accumulate in pass 2
        }
    }

    // Pass 2: continentality — fraction of land cells in a ~10° radius
    {
        const int windowLat = std::max(1, latB / 18);  // ~10° in lat
        const int windowLon = std::max(1, lonS / 36);  // ~10° in lon
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                int land = 0, total = 0;
                for (int dla = -windowLat; dla <= windowLat; ++dla)
                {
                    for (int dlo = -windowLon; dlo <= windowLon; ++dlo)
                    {
                        const int nla = std::clamp(la + dla, 0, latB - 1);
                        const int nlo = ((lo + dlo) + lonS) % lonS;
                        if (!Cell(nla, nlo).isOcean) ++land;
                        ++total;
                    }
                }
                Cell(la, lo).continentality = total > 0
                    ? static_cast<double>(land) / total
                    : 0.0;
            }
        }
    }

    // Pass 3: precipitation, humidity, snow
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const double absLat = std::abs(c.latitudeDegrees);

            // Circulation wetness factor
            const double circW = CirculationWetness(absLat, rot);

            // Base moisture: scales with volatile inventory
            const double moistBase = std::clamp(volFrac / 0.02, 0.1, 5.0);

            // Continental drying: far from ocean = less moisture advection
            const double contDry = 1.0 - c.continentality * 0.6;

            // Local moisture noise — simulates unresolved weather patterns
            const double moistNoise = fBmGrid(la, lo, latB, lonS, seed ^ 0x1234abcd, 5, 10, 5);

            // Orographic lifting: windward face of slopes gets more rain
            // lee-side (rain shadow): drier
            // Approximate: slope in the direction toward equator = windward
            const double orographic = c.isOcean ? 0.0
                : std::max(0.0, c.slopeNormalized * 0.5 - c.tectonicStress * 0.1);

            // Combined humidity
            c.humidityNormalized = std::clamp(
                circW * contDry * moistBase * (0.5 + moistNoise * 0.5) * (1.0 + orographic),
                0.0, 1.0);

            // Clausius-Clapeyron: cold air holds much less water
            const double tempK = c.temperatureCelsius + 273.15;
            const double satFactor = std::clamp(std::exp((tempK - 273.15) * 0.065) / 10.0, 0.01, 2.0);

            // Precipitation: humidity × saturation × pressure
            c.precipitationMm = std::max(0.0,
                c.humidityNormalized * 1400.0 * pressureScale * satFactor);

            // Rain shadow on lee side — subtract from elevation slope
            // Cells that have mountains upwind receive less moisture (rough approximation)
            // We detect this as high slope on an otherwise dry-circulation cell
            if (!c.isOcean && c.slopeNormalized > 0.3 && circW < 0.45)
                c.precipitationMm *= std::clamp(1.0 - c.slopeNormalized * 0.7, 0.2, 1.0);

            // Soil moisture: precip minus evapotranspiration
            const double pet = std::max(0.0, c.temperatureCelsius * 15.0); // crude PET
            c.soilMoisture = std::clamp(c.precipitationMm / (c.precipitationMm + pet + 1.0), 0.0, 1.0);

            // Dry season months
            c.drySeasonMonths = DrySeasonMonths(c.precipitationMm, circW);

            // Snow days: positive at T near 0°C, more with high precipitation
            const double snowTemp = std::clamp((-c.temperatureCelsius) / 10.0, 0.0, 1.0);
            c.snowDays = snowTemp * std::clamp(c.precipitationMm / 800.0, 0.0, 1.0) * 200.0;
        }
    }
}

// ==========================================================================
// Stage 5 — Soils
// ==========================================================================
void PlanetMap::GenerateSoils()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    const double ageFrac = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            if (c.isOcean)
            {
                c.soilFertility  = 0.0;
                c.soilDepthNorm  = 0.0;
                continue;
            }

            // Folded sedimentary rock in mountain belts breaks into fertile soil
            // if climate allows
            c.soilFertility = SoilFertility(
                c.surfaceRock, c.temperatureCelsius, c.precipitationMm, ageFrac);
            c.soilDepthNorm = SoilDepth(
                c.surfaceRock, c.precipitationMm, c.slopeNormalized, ageFrac);

            // River valleys: deposited alluvium → very fertile, deep soil
            if (c.hasRiver && c.slopeNormalized < 0.2)
            {
                c.soilFertility  = std::min(1.0, c.soilFertility * 1.5);
                c.soilDepthNorm  = std::min(1.0, c.soilDepthNorm  * 1.8);
            }

            // Volcanic terrain: initially barren, but highly fertile after weathering
            if (c.isVolcanicZone)
            {
                const double weatheredFrac = std::clamp(ageFrac * 2.0, 0.0, 1.0);
                c.soilFertility = Lerp(0.1, 0.85, weatheredFrac);
                c.soilDepthNorm = Lerp(0.1, 0.6,  weatheredFrac);
            }

            // Permafrost / very cold: thin, infertile soil
            if (c.temperatureCelsius < -5.0)
            {
                const double coldFrac = std::clamp((-c.temperatureCelsius - 5.0) / 25.0, 0.0, 1.0);
                c.soilFertility *= (1.0 - coldFrac * 0.8);
                c.soilDepthNorm *= (1.0 - coldFrac * 0.7);
            }
        }
    }
}

// ==========================================================================
// Stage 6 — Biome classification
// ==========================================================================
void PlanetMap::ClassifyBiomes()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    for (int la = 0; la < latB; ++la)
        for (int lo = 0; lo < lonS; ++lo)
            Cell(la, lo).biome = ClassifyBiome(Cell(la, lo));
}

} // namespace Infinitesia
