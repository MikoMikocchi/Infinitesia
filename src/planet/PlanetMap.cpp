// PlanetMap.cpp
// Multi-stage deterministic planet generation pipeline.
//
// Stages (executed once in constructor, results cached in m_grid):
//   1. Tectonics   — Euler-pole plate kinematics, Voronoi assignment,
//                    correct boundary classification, crust model
//   2. Terrain     — tectonic uplift + fBm detail + erosion
//   3. Climate     — stellar flux, latitude insolation, circulation zones,
//                    orographic effects, continentality
//   4. Hydrology   — drainage accumulation, rivers, lakes (after climate)
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

double fBmGrid(int la, int lo, int latB, int lonS, std::uint32_t seed,
               int baseLat = 6, int baseLon = 12, int octaves = 6)
{
    const double nLat = (la + 0.5) / latB;
    const double nLon = (lo + 0.5) / lonS;
    return fBm(nLat, nLon, seed, baseLat, baseLon, octaves);
}

// --------------------------------------------------------------------------
// Spherical geometry
// --------------------------------------------------------------------------
struct Vec3d { double x = 0.0, y = 0.0, z = 0.0; };

Vec3d LatLonToCart(double latDeg, double lonDeg)
{
    const double lat = latDeg * std::numbers::pi / 180.0;
    const double lon = lonDeg * std::numbers::pi / 180.0;
    return { std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
}

double Dot(const Vec3d& a, const Vec3d& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

Vec3d Cross(const Vec3d& a, const Vec3d& b)
{
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

Vec3d Normalize(const Vec3d& v)
{
    const double len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len < 1e-12) return { 0.0, 0.0, 0.0 };
    return { v.x/len, v.y/len, v.z/len };
}

// Great-circle angular distance in degrees
double GCDistance(double la1, double lo1, double la2, double lo2)
{
    const Vec3d a = LatLonToCart(la1, lo1);
    const Vec3d b = LatLonToCart(la2, lo2);
    const double dot = std::clamp(Dot(a, b), -1.0, 1.0);
    return std::acos(dot) * 180.0 / std::numbers::pi;
}

// --------------------------------------------------------------------------
// Euler Pole plate kinematics
//
// Each plate rotates about an Euler pole (a unit vector on the sphere)
// with angular velocity omegaRadPerMy [rad/Myr].
//
// Local velocity at a surface point r (unit vector) in Cartesian space:
//   v_cart = omega_vec × r,   where omega_vec = omega * pole_unit_vector
//
// We project v_cart onto the local lat/lon tangent frame to get
// vLat (deg/My, positive northward) and vLon (deg/My, positive eastward).
//
// This is physically correct on a sphere: one degree at the equator and one
// degree near the poles both map to the same angular displacement.
// --------------------------------------------------------------------------
struct EulerPole
{
    Vec3d  axis{};            // unit vector in 3D Cartesian space
    double omegaRadPerMy = 0.0; // angular velocity, radians per million years
};

struct PlateKinematics
{
    EulerPole euler{};
};

// Local surface velocity in deg/Myr at a given lat/lon point.
// Returns { vLat [deg/Myr northward], vLon [deg/Myr eastward] }.
std::pair<double,double> EulerVelocityDegPerMy(
    const EulerPole& pole,
    double latDeg, double lonDeg)
{
    // r: unit vector of the point on the sphere
    const Vec3d r = LatLonToCart(latDeg, lonDeg);

    // omega_vec = omega * axis
    const Vec3d omega{ pole.axis.x * pole.omegaRadPerMy,
                       pole.axis.y * pole.omegaRadPerMy,
                       pole.axis.z * pole.omegaRadPerMy };

    // v_cart = omega × r  (Cartesian velocity, radians/My as magnitude)
    const Vec3d v = Cross(omega, r);

    // Local tangent frame:
    //   north = d(r)/d(lat), i.e. derivative of LatLonToCart w.r.t. lat
    //   east  = d(r)/d(lon) / cos(lat)
    const double lat = latDeg * std::numbers::pi / 180.0;
    const double lon = lonDeg * std::numbers::pi / 180.0;
    const double cosLat = std::cos(lat);
    const double sinLat = std::sin(lat);
    const double cosLon = std::cos(lon);
    const double sinLon = std::sin(lon);

    // north tangent (unnormalised, but already unit-length)
    const Vec3d north{ -sinLat * cosLon, cosLat, -sinLat * sinLon };
    // east tangent (unit-length for cos(lat) != 0)
    // For polar singularity we return 0
    const Vec3d east{ -sinLon, 0.0, cosLon };

    // Project v onto north and east → rad/My
    const double vNorthRad = Dot(v, north);
    const double vEastRad  = (cosLat > 1e-6) ? Dot(v, east) : 0.0;

    // Convert rad/My → deg/My: multiply by (180/π)
    constexpr double kRadToDeg = 180.0 / std::numbers::pi;
    return { vNorthRad * kRadToDeg, vEastRad * kRadToDeg };
}

// --------------------------------------------------------------------------
// Tectonic plate descriptor
// --------------------------------------------------------------------------
struct TectonicPlate
{
    int    id          = 0;
    double centerLat   = 0.0;
    double centerLon   = 0.0;
    bool   isOceanic   = false;

    double crustDensity       = 2900.0; // kg/m³
    double crustThicknessKm   = 7.0;    // km

    PlateKinematics kinematics{};
};

// --------------------------------------------------------------------------
// Boundary classification using Euler-pole velocities
//
// normalComponent:  positive → A moves away from B → Divergent
//                   negative → A moves toward  B → Convergent
//
// tangential / normal ratio distinguishes Transform from pure Convergent/Divergent.
//
// Thresholds scaled to actual velocity magnitudes (~0.01–0.1 deg/Myr).
// A boundary is Convergent/Divergent when the normal component exceeds
// a threshold AND dominates over the tangential component.
// --------------------------------------------------------------------------
enum class BoundaryType { Divergent, Convergent, Transform };

BoundaryType ClassifyBoundaryFromVelocity(
    double vLatA, double vLonA,   // deg/My for plate A at boundary point
    double vLatB, double vLonB,   // deg/My for plate B at boundary point
    double normalLat,             // lat-component of unit boundary normal (A→B)
    double normalLon)             // lon-component of unit boundary normal (A→B)
{
    // Relative velocity: A with respect to B
    const double dvLat = vLatA - vLatB;
    const double dvLon = vLonA - vLonB;

    // Component along boundary normal:
    //   positive → A moves away from B (Divergent)
    //   negative → A moves toward  B  (Convergent)
    const double normalComponent = dvLat * normalLat + dvLon * normalLon;

    // Tangential (strike-slip) magnitude
    const double dv2 = dvLat*dvLat + dvLon*dvLon;
    const double tangential = std::sqrt(std::max(0.0, dv2 - normalComponent*normalComponent));

    // Minimum threshold: ignore sub-noise relative motion
    const double totalRelSpeed = std::sqrt(dv2);
    constexpr double kMinSpeed = 0.004; // deg/My — below this, treat as Transform
    if (totalRelSpeed < kMinSpeed)
        return BoundaryType::Transform;

    // Transform when strike-slip dominates: |tangential| > 2 * |normal|
    if (tangential > std::abs(normalComponent) * 2.0)
        return BoundaryType::Transform;

    // Scale-aware thresholds: 25% of the total relative speed
    // This correctly handles both fast and slow plates without hard-coded units.
    const double threshold = totalRelSpeed * 0.25;

    if (normalComponent < -threshold)
        return BoundaryType::Convergent;
    if (normalComponent >  threshold)
        return BoundaryType::Divergent;

    return BoundaryType::Transform;
}

enum class BoundarySubtype
{
    MidOceanRidge,          // Divergent,  oceanic–oceanic
    ContinentalRift,        // Divergent,  continental–continental or mixed
    OceanicSubduction,      // Convergent, oceanic–oceanic → island arc
    ContinentalSubduction,  // Convergent, oceanic under continental → volcanic arc + trench
    ContinentalCollision,   // Convergent, continental–continental → orogen
    TransformFault,         // Transform, any combination
};

BoundarySubtype DetailedBoundary(BoundaryType bt, bool aOceanic, bool bOceanic)
{
    switch (bt)
    {
    case BoundaryType::Divergent:
        return (aOceanic && bOceanic)
            ? BoundarySubtype::MidOceanRidge
            : BoundarySubtype::ContinentalRift;

    case BoundaryType::Convergent:
        if (!aOceanic && !bOceanic) return BoundarySubtype::ContinentalCollision;
        if ( aOceanic &&  bOceanic) return BoundarySubtype::OceanicSubduction;
        return BoundarySubtype::ContinentalSubduction;

    case BoundaryType::Transform:
        return BoundarySubtype::TransformFault;
    }
    return BoundarySubtype::TransformFault;
}

// --------------------------------------------------------------------------
// Airy Isostasy
// h = T * (ρ_mantle - ρ_crust) / ρ_mantle  −  offset
// Offset calibrated so that normal ocean floor (T=7, ρ=2900) ≈ -3.5 km.
// --------------------------------------------------------------------------
constexpr double kMantle          = 3300.0;
constexpr double kOceanicRho      = 2900.0;
constexpr double kContinentalRho  = 2750.0;
constexpr double kIsostasyOffset  = 4.35; // km

double AiryIsostasyKm(double crustThicknessKm, double crustDensity)
{
    return crustThicknessKm * (kMantle - crustDensity) / kMantle - kIsostasyOffset;
}

// Oceanic crust subsides as it cools after spreading.
// depth_ridge ≈ −2.6 km, k ≈ 0.35 km/√Ma (empirical fit to Earth ocean floor).
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
    const double sinL = std::sin(lat);
    const double tiltBoost = std::max(0.0, 1.0 - sinL*sinL*(1.0 - sinT*sinT));
    const double insol = (1.0 - sinT)*cosL + sinT*std::sqrt(tiltBoost)*0.5;
    return std::clamp(insol, 0.0, 1.0);
}

// Seasonal amplitude: difference summer–winter temperature [°C]
double SeasonalAmplitude(double latDeg, double tiltDeg, double rotHours)
{
    const double lat  = std::abs(latDeg) * std::numbers::pi / 180.0;
    const double tilt = tiltDeg * std::numbers::pi / 180.0;
    const double baseAmplitude = std::sin(lat) * std::sin(tilt) * 50.0;
    const double rotFactor = std::clamp(std::log(rotHours / 24.0 + 1.0) / 1.0, 0.5, 1.5);
    return std::clamp(baseAmplitude * rotFactor, 0.0, 60.0);
}

// Physically-grounded equilibrium temperature.
//
// Stefan-Boltzmann for a planet with Bond albedo ≈ 0.30:
//   T_eff = 255 K × flux^0.25   (255 K = Earth effective emission temperature)
//
// Surface temperature adds the greenhouse delta separately, so that
// GreenhouseK() is not implicitly baked into the base.
double EquilibriumTempK(double flux)
{
    // 255 K is the effective emission temperature of Earth at 1 AU
    // (Bond albedo ~0.30, Solar constant ~1361 W/m²).
    return 255.0 * std::pow(std::max(0.01, flux), 0.25);
}

// Greenhouse warming from atmospheric pressure and volatile inventory
double GreenhouseK(double pressKPa, double volFrac)
{
    if (pressKPa <= 0.0) return 0.0;
    const double vScale = std::clamp(volFrac / 0.02, 0.1, 5.0);
    // Earth: 101 kPa, volFrac~0.02 → ~33 K (matches observed greenhouse delta)
    return 33.0 * std::log(pressKPa / 101.325 + 1.0) * std::sqrt(vScale);
}

// Altitude lapse cooling [K]
double LapseK(double elevNorm, double gravMs2)
{
    const double elevM = std::max(0.0, elevNorm) * 9000.0;
    const double lapse = 0.0065 * (gravMs2 / 9.81);
    return elevM * lapse;
}

// Zonal atmospheric circulation wetness [0..1]
// Models Hadley / Ferrel / Polar cells with rotation-period scaling.
double CirculationWetness(double absLatDeg, double rotHours)
{
    const double cellScale = std::clamp(24.0 / rotHours, 0.4, 2.0);
    const double lat = absLatDeg * std::numbers::pi / 180.0;

    // ITCZ: ascending moist air near equator
    const double itczWidth = 0.12 * cellScale;
    const double itcz = std::exp(-std::pow(lat, 2.0) / itczWidth);

    // Subtropical dry belt (~25°): descending dry air
    const double subC = 25.0 / cellScale * std::numbers::pi / 180.0;
    const double sub  = std::exp(-std::pow(lat - subC, 2.0) / 0.04);

    // Mid-latitude frontal zone (~55°): ascending moist air
    const double midC = 55.0 / cellScale * std::numbers::pi / 180.0;
    const double mid  = 0.60 * std::exp(-std::pow(lat - midC, 2.0) / 0.05);

    // Polar dry (>65°): descending dry air
    const double polarDry = std::max(0.0, (absLatDeg - 65.0) / 25.0);

    const double raw = itcz - sub * 0.75 + mid - polarDry * 0.5;
    return std::clamp(raw * 0.5 + 0.5, 0.0, 1.0);
}

// Dry-season months derived from the circulation wetness signal
double DrySeasonMonths(double precipMm, double circulationWetness)
{
    const double dryFraction = std::clamp(1.0 - circulationWetness * 1.4, 0.0, 1.0);
    return dryFraction * 7.0;
}

// --------------------------------------------------------------------------
// Soil helpers
// --------------------------------------------------------------------------
double SoilFertility(RockType rock, double tempC, double precipMm, double planetAgeFrac)
{
    double base = 0.2;
    switch (rock) {
        case RockType::VolcanicIgneous:    base = 0.65; break;
        case RockType::BasalticOceanic:    base = 0.35; break;
        case RockType::SedimentaryBasin:   base = 0.55; break;
        case RockType::FoldedSedimentary:  base = 0.45; break;
        case RockType::GraniticShield:     base = 0.30; break;
        case RockType::MetamorphicBelt:    base = 0.25; break;
    }
    const double tempFactor = std::clamp((tempC + 5.0) / 30.0, 0.0, 1.0);
    const double wetFactor  = std::clamp(precipMm / 900.0, 0.0, 1.0);
    const double weathering = tempFactor * wetFactor;
    const double ageFactor  = std::clamp(planetAgeFrac, 0.0, 1.0);
    const double leaching   = (tempC > 22.0 && precipMm > 1500.0) ? 0.6 : 1.0;
    return std::clamp(base * (0.3 + 0.7*weathering) * (0.5 + 0.5*ageFactor) * leaching, 0.0, 1.0);
}

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
    const double wetFactor   = std::clamp(precipMm / 1200.0, 0.0, 1.0);
    const double slopeFactor = 1.0 - std::clamp(slope * 1.5, 0.0, 0.9);
    const double ageFactor   = std::clamp(planetAgeFrac, 0.1, 1.0);
    return std::clamp(base * wetFactor * slopeFactor * ageFactor, 0.0, 1.0);
}

// --------------------------------------------------------------------------
// Biome classification — Köppen-extended, 23 types
// --------------------------------------------------------------------------
Biome ClassifyBiome(const PlanetRegion& r)
{
    const double elevKm = r.elevationNormalized * 9.0;
    const double t      = r.temperatureCelsius;
    const double p      = r.precipitationMm;
    const double dry    = r.drySeasonMonths;
    const double snow   = r.snowDays;

    // Aquatic
    if (r.isOcean)
    {
        if (t < -2.0)       return Biome::PolarSea;
        if (elevKm < -4.5)  return Biome::DeepOcean;
        if (t > 20.0 && elevKm > -0.4) return Biome::CoralReef;
        return Biome::ShallowSea;
    }

    if (r.isLake) return Biome::ShallowSea;

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
    if (t < -20.0) return Biome::IceCap;
    if (t < -15.0) return Biome::Glacier;

    // Tundra
    if (t < -2.0)
        return (p > 350.0) ? Biome::WetTundra : Biome::Tundra;

    // Boreal taiga
    if (t < 7.0)
    {
        if (p < 250.0) return Biome::Tundra;
        if (r.soilMoisture > 0.7) return Biome::BorealWetland;
        return Biome::BorealForest;
    }

    // Cold desert / steppe
    if (t < 14.0 && p < 200.0) return Biome::ColdDesert;
    if (t < 14.0 && p < 450.0) return Biome::TemperateSteppe;

    // Temperate
    if (t < 18.0)
    {
        if (dry > 3.0 && p > 300.0 && p < 900.0) return Biome::MediterraneanShrub;
        if (p > 700.0) return Biome::TemperateDeciduous;
        if (p > 400.0) return Biome::TemperateMixed;
        if (p > 200.0) return Biome::TemperateSteppe;
        return Biome::ColdDesert;
    }

    // Hot desert
    if (p < 200.0) return Biome::HotDesert;

    // Subtropical / tropical
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

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const auto ctr = m_planet.GetRegionCenter(la, lo);
            c.latitudeIndex    = la;
            c.longitudeIndex   = lo;
            c.seed             = m_planet.DeriveRegionSeed(la, lo);
            c.latitudeDegrees  = ctr.latitudeDegrees;
            c.longitudeDegrees = ctr.longitudeDegrees;
        }
    }

    // Pipeline order: Climate must precede Hydrology so that
    // river generation has access to precipitation data.
    GenerateTectonics();
    GenerateTerrain();
    GenerateClimate();    // moved before Hydrology
    GenerateHydrology();  // now uses precipitationMm to filter rivers
    GenerateSoils();
    ClassifyBiomes();
}

// --------------------------------------------------------------------------
// Accessors
// --------------------------------------------------------------------------

const Planet&           PlanetMap::GetPlanet()        const noexcept { return m_planet; }
const PlanetParameters& PlanetMap::GetParameters()    const noexcept { return m_planet.GetParameters(); }
const ClimateInputs&    PlanetMap::GetClimateInputs() const noexcept { return m_climate; }
int PlanetMap::GetLatitudeBandCount()     const noexcept { return m_planet.GetParameters().latitudeBands; }
int PlanetMap::GetLongitudeSegmentCount() const noexcept { return m_planet.GetParameters().longitudeSegments; }

const PlanetRegion& PlanetMap::GetRegion(int la, int lo) const noexcept
{
    return Cell(std::clamp(la, 0, LatBands()-1), std::clamp(lo, 0, LonSegs()-1));
}

double PlanetMap::SampleElevation(int la, int lo) const { return GetRegion(la, lo).elevationNormalized; }
bool   PlanetMap::IsOcean(int la, int lo)         const { return GetRegion(la, lo).isOcean; }

PlanetRegion&       PlanetMap::Cell(int la, int lo)       { return m_grid[static_cast<std::size_t>(la) * LonSegs() + lo]; }
const PlanetRegion& PlanetMap::Cell(int la, int lo) const { return m_grid[static_cast<std::size_t>(la) * LonSegs() + lo]; }

int PlanetMap::LatBands() const noexcept { return m_planet.GetParameters().latitudeBands; }
int PlanetMap::LonSegs()  const noexcept { return m_planet.GetParameters().longitudeSegments; }

// ==========================================================================
// Stage 1 — Tectonics
//
// Key changes from previous version:
//   • Plates use Euler-pole kinematics instead of flat lat/lon velocity.
//     Each plate rotates about a random axis; local velocity is computed via
//     v = ω × r and projected onto the lat/lon tangent frame.
//   • Boundary classification uses relative speed and normal/tangential ratio
//     instead of hard-coded 0.3 deg/My thresholds that were unreachable.
//   • crustAge for oceanic cells is derived from distance to the nearest
//     Divergent boundary (actual MOR/rift location), not distance to the
//     plate centre.
//   • Continental crust age is assigned regionally via fBm, not per-cell random.
// ==========================================================================
void PlanetMap::GenerateTectonics()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    const std::uint32_t seed = m_planet.GetSeed();

    // -----------------------------------------------------------------------
    // Number of plates from internal heat production.
    // More massive / younger planet → more heat → more convection cells.
    // Earth (~6e24 kg, 4.5 Gyr) → 12–15 major plates.
    // -----------------------------------------------------------------------
    const double massFrac  = m_planet.GetParameters().massKilograms / 5.972e24;
    const double ironFrac  = m_planet.GetParameters().bulkIronMassFraction;
    const double ageFrac   = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);
    // Older planets lose internal heat → fewer, larger, slower plates
    const double heatScale = std::sqrt(massFrac) * (1.0 - 0.4 * ageFrac);
    const int numPlates    = static_cast<int>(std::clamp(5.0 + heatScale * 11.0, 5.0, 16.0));

    // -----------------------------------------------------------------------
    // Continental fraction: volatile-rich worlds have more ocean.
    // -----------------------------------------------------------------------
    const double continentalFraction = std::clamp(
        ironFrac * 2.2 - (m_climate.volatileMassFraction / 0.02 - 1.0) * 0.1,
        0.25, 0.75);

    // -----------------------------------------------------------------------
    // Place plate centres on a Fibonacci spiral (uniform sphere distribution)
    // -----------------------------------------------------------------------
    std::vector<TectonicPlate> plates(static_cast<std::size_t>(numPlates));
    const double goldenAngle = std::numbers::pi * (3.0 - std::sqrt(5.0));

    for (int p = 0; p < numPlates; ++p)
    {
        const auto pu = static_cast<std::uint32_t>(p);
        const double y     = 1.0 - (p / (numPlates - 1.0)) * 2.0;
        const double theta = goldenAngle * p + RandRange(seed, pu*13u+1u, -0.25, 0.25);
        plates[p].id        = p;
        plates[p].centerLat = std::asin(std::clamp(y, -1.0, 1.0)) * 180.0 / std::numbers::pi;
        plates[p].centerLon = std::fmod(theta * 180.0 / std::numbers::pi + 360.0, 360.0);

        plates[p].isOceanic = RandUnit(seed, pu*7u+1u) > continentalFraction;

        if (plates[p].isOceanic)
        {
            plates[p].crustDensity     = 2870.0 + RandRange(seed, pu*7u+3u, -40.0,  80.0);
            plates[p].crustThicknessKm = RandRange(seed, pu*7u+4u,   5.0,  10.0);
        }
        else
        {
            plates[p].crustDensity     = 2720.0 + RandRange(seed, pu*7u+3u, -40.0,  60.0);
            plates[p].crustThicknessKm = RandRange(seed, pu*7u+4u,  28.0,  50.0);
        }

        // -----------------------------------------------------------------------
        // Euler pole: random rotation axis + angular velocity.
        //
        // Earth plates: 1–10 cm/yr ≈ 0.01–0.09 deg/Myr (equatorial arc).
        // In Euler-pole terms: ω = v_equatorial / R_equatorial.
        // For a unit sphere, R=1 rad → ω_rad/Myr ≈ v_deg/Myr × π/180.
        //
        // We generate ω in [0.01, 0.09] deg/Myr and convert to rad/Myr.
        // -----------------------------------------------------------------------
        const double speedDegPerMy = RandRange(seed, pu*7u+5u, 0.01, 0.09) * heatScale;

        // Random Euler pole axis (uniform on sphere via rejection-free method)
        const double axisLat = std::asin(RandRange(seed, pu*7u+6u, -1.0, 1.0)) * 180.0 / std::numbers::pi;
        const double axisLon = RandRange(seed, pu*7u+7u, 0.0, 360.0);
        plates[p].kinematics.euler.axis = LatLonToCart(axisLat, axisLon);
        // Convert deg/My to rad/My
        plates[p].kinematics.euler.omegaRadPerMy = speedDegPerMy * std::numbers::pi / 180.0;
    }

    // -----------------------------------------------------------------------
    // Voronoi assignment: each cell → nearest plate (great-circle distance).
    // Record second-nearest for boundary detection.
    // -----------------------------------------------------------------------
    struct CellAssignment
    {
        int best = 0, second = 0;
        double bestDist = 1e18, secondDist = 1e18;
    };
    std::vector<CellAssignment> assignments(static_cast<std::size_t>(latB) * lonS);

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const auto& c = Cell(la, lo);
            auto& a = assignments[static_cast<std::size_t>(la)*lonS + lo];
            for (int p = 0; p < numPlates; ++p)
            {
                const double d = GCDistance(c.latitudeDegrees, c.longitudeDegrees,
                                            plates[p].centerLat, plates[p].centerLon);
                if (d < a.bestDist)
                {
                    a.secondDist = a.bestDist; a.second = a.best;
                    a.bestDist   = d;          a.best   = p;
                }
                else if (d < a.secondDist)
                {
                    a.secondDist = d; a.second = p;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Pass 1: assign plate data and classify boundaries
    // -----------------------------------------------------------------------

    // We track which cells are classified as Divergent boundaries,
    // so we can compute realistic crustAge in Pass 2.
    std::vector<bool> isDivergentBoundary(static_cast<std::size_t>(latB) * lonS, false);

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const auto& a  = assignments[static_cast<std::size_t>(la)*lonS + lo];
            const TectonicPlate& plateA = plates[a.best];
            const TectonicPlate& plateB = plates[a.second];

            c.tectonicPlateId = a.best;

            // Boundary proximity: smooth falloff over ~4° arc
            constexpr double kBoundaryWidth = 4.0;
            c.tectonicStress = SmootherStep(
                std::clamp(1.0 - (a.secondDist - a.bestDist) / kBoundaryWidth, 0.0, 1.0));

            // Boundary normal in lat/lon tangent space (approximate)
            const double dLat = plateB.centerLat - plateA.centerLat;
            const double dLon = plateB.centerLon - plateA.centerLon;
            const double dLen = std::sqrt(dLat*dLat + dLon*dLon);
            const double normalLat = dLen > 0.0 ? dLat / dLen : 0.0;
            const double normalLon = dLen > 0.0 ? dLon / dLen : 1.0;

            // Compute Euler-pole velocities at this cell's location
            auto [vLatA, vLonA] = EulerVelocityDegPerMy(
                plateA.kinematics.euler, c.latitudeDegrees, c.longitudeDegrees);
            auto [vLatB, vLonB] = EulerVelocityDegPerMy(
                plateB.kinematics.euler, c.latitudeDegrees, c.longitudeDegrees);

            const BoundaryType bt = ClassifyBoundaryFromVelocity(
                vLatA, vLonA, vLatB, vLonB, normalLat, normalLon);
            const BoundarySubtype bst = DetailedBoundary(bt, plateA.isOceanic, plateB.isOceanic);

            if (bt == BoundaryType::Divergent)
                isDivergentBoundary[static_cast<std::size_t>(la)*lonS + lo] = true;

            // ----------------------------------------------------------------
            // Interior crust properties
            // ----------------------------------------------------------------
            c.crustThickness = plateA.crustThicknessKm;
            // crustAge for interior cells: assigned after boundary pass (Pass 2)
            // Temporary: random placeholder, will be overwritten
            c.crustAge = RandRange(seed,
                static_cast<std::uint32_t>(la*lonS + lo) ^ 0xbeef0000u, 0.0, 1.0);

            if (plateA.isOceanic)
            {
                c.surfaceRock    = RockType::BasalticOceanic;
                c.isVolcanicZone = false;
            }
            else
            {
                // Continental interior age via low-frequency fBm: coherent regions
                // (old cratons vs younger basins), not random per cell
                const double ageSample = fBmGrid(la, lo, latB, lonS, seed ^ 0xcafe0000u, 3, 6, 4);
                c.crustAge = ageSample; // [0..1], spatially coherent
                c.surfaceRock    = (ageSample > 0.65)
                    ? RockType::GraniticShield    // old craton
                    : RockType::SedimentaryBasin; // younger basin or platform
                c.isVolcanicZone = false;
            }

            // ----------------------------------------------------------------
            // Boundary-zone overrides (applied proportional to stress)
            // ----------------------------------------------------------------
            if (c.tectonicStress > 0.25)
            {
                // Relative speed at this point (for thickening scale)
                const double relSpeed = std::hypot(vLatA - vLatB, vLonA - vLonB);

                switch (bst)
                {
                case BoundarySubtype::ContinentalCollision:
                {
                    // Collision thickens crust → mountains via isostasy
                    // Real Himalaya: ~35 km → ~70 km collision zone
                    const double thickeningFactor =
                        1.0 + c.tectonicStress * 0.9 * std::max(relSpeed / 0.05, 1.0);
                    c.crustThickness = plateA.crustThicknessKm * std::clamp(thickeningFactor, 1.0, 2.2);
                    c.surfaceRock    = RockType::MetamorphicBelt;
                    c.isVolcanicZone = false;
                    break;
                }

                case BoundarySubtype::ContinentalSubduction:
                    // Oceanic plate dips under continental → compression + arc volcanism
                    c.crustThickness = plateA.isOceanic
                        ? 5.0
                        : plateA.crustThicknessKm * (1.0 + c.tectonicStress * 0.35);
                    c.surfaceRock    = RockType::VolcanicIgneous;
                    c.isVolcanicZone = true;
                    break;

                case BoundarySubtype::OceanicSubduction:
                    // Island arc above subducting oceanic slab
                    c.surfaceRock    = RockType::VolcanicIgneous;
                    c.isVolcanicZone = true;
                    c.crustThickness = 12.0 + c.tectonicStress * 8.0;
                    break;

                case BoundarySubtype::MidOceanRidge:
                    // Thin, hot, young basalt at spreading centre
                    c.crustThickness = 4.0 + (1.0 - c.tectonicStress) * 3.0;
                    c.crustAge       = 0.0; // brand new crust
                    c.surfaceRock    = RockType::BasalticOceanic;
                    c.isVolcanicZone = true;
                    break;

                case BoundarySubtype::ContinentalRift:
                    // Thinning crust, future ocean, active volcanism
                    c.crustThickness = plateA.crustThicknessKm * (1.0 - c.tectonicStress * 0.4);
                    c.surfaceRock    = RockType::BasalticOceanic;
                    c.isVolcanicZone = true;
                    break;

                case BoundarySubtype::TransformFault:
                    c.surfaceRock    = plateA.isOceanic
                        ? RockType::BasalticOceanic
                        : RockType::FoldedSedimentary;
                    c.isVolcanicZone = false;
                    break;
                }
            }

            // Oceanic intraplate hotspot seamounts (Hawaii-style)
            if (plateA.isOceanic && c.tectonicStress < 0.1 &&
                RandUnit(seed ^ 0xbabe, la, lo) < 0.006)
            {
                c.isVolcanicZone = true;
                c.surfaceRock    = RockType::VolcanicIgneous;
                c.crustThickness = 18.0;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Pass 2: crustAge for oceanic crust based on distance to nearest MOR.
    //
    // Previous approach: crustAge = bestDist / 90, where bestDist is distance
    // to plate centre. This placed the MOR at the plate centre, which is
    // arbitrary and often wrong.
    //
    // New approach: find all Divergent boundary cells (real MOR/rift locations),
    // then set crustAge proportional to distance to the nearest such cell.
    // This correctly models seafloor spreading: youngest crust at the ridge,
    // oldest crust far from any spreading centre.
    //
    // Continental crust age was already set from spatially-coherent fBm above.
    // -----------------------------------------------------------------------
    {
        // Collect MOR cell indices (Divergent boundary cells)
        std::vector<int> morCells;
        morCells.reserve(512);
        for (int la = 0; la < latB; ++la)
            for (int lo = 0; lo < lonS; ++lo)
                if (isDivergentBoundary[static_cast<std::size_t>(la)*lonS + lo])
                    morCells.push_back(la * lonS + lo);

        if (!morCells.empty())
        {
            // For each oceanic interior cell: find distance to nearest MOR cell.
            // Max oceanic crust age on Earth ≈ 180 Ma, corresponding to a spreading
            // distance of roughly 80–100° great-circle arc (slow plates over long time).
            constexpr double kMaxAgeDistDeg = 80.0;

            for (int la = 0; la < latB; ++la)
            {
                for (int lo = 0; lo < lonS; ++lo)
                {
                    auto& c = Cell(la, lo);
                    const auto& a = assignments[static_cast<std::size_t>(la)*lonS + lo];

                    // Only recompute for oceanic interior cells
                    if (!plates[a.best].isOceanic) continue;
                    if (isDivergentBoundary[static_cast<std::size_t>(la)*lonS + lo]) continue;

                    double minDist = 1e18;
                    for (int morIdx : morCells)
                    {
                        const int morLa = morIdx / lonS;
                        const int morLo = morIdx % lonS;
                        const double d = GCDistance(
                            c.latitudeDegrees, c.longitudeDegrees,
                            Cell(morLa, morLo).latitudeDegrees,
                            Cell(morLa, morLo).longitudeDegrees);
                        if (d < minDist) minDist = d;
                    }

                    // Normalise: 0 = at MOR (youngest), 1 = far from any MOR (oldest)
                    c.crustAge = std::clamp(minDist / kMaxAgeDistDeg, 0.0, 1.0);
                }
            }
        }
        // If no Divergent boundaries exist (very stable planet), fall back to
        // distance-to-plate-centre approximation for oceanic cells.
        else
        {
            for (int la = 0; la < latB; ++la)
            {
                for (int lo = 0; lo < lonS; ++lo)
                {
                    auto& c = Cell(la, lo);
                    const auto& a = assignments[static_cast<std::size_t>(la)*lonS + lo];
                    if (plates[a.best].isOceanic)
                        c.crustAge = std::clamp(a.bestDist / 90.0, 0.0, 1.0);
                }
            }
        }
    }
}

// ==========================================================================
// Stage 2 — Terrain
// ==========================================================================
void PlanetMap::GenerateTerrain()
{
    const int latB = LatBands();
    const int lonS = LonSegs();
    const std::uint32_t seed = m_planet.GetSeed();

    const double ageFrac   = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);
    const double gravScale = m_climate.gravityMetersPerSecondSquared / 9.81;

    // -----------------------------------------------------------------------
    // Pass 1: raw elevation from isostasy / bathymetry
    // -----------------------------------------------------------------------
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
                // Pure oceanic crust: depth from bathymetric cooling.
                // crustAge now correctly represents distance from MOR.
                elev = OceanicBathymetryKm(c.crustAge, 180.0);
            }
            else
            {
                elev = AiryIsostasyKm(c.crustThickness,
                    c.surfaceRock == RockType::BasalticOceanic
                        ? kOceanicRho : kContinentalRho);
            }

            // Subduction trench
            if (c.surfaceRock == RockType::BasalticOceanic &&
                c.tectonicStress > 0.4 && !c.isVolcanicZone)
            {
                const double trenchDepth = -8.0 - c.tectonicStress * 3.0;
                elev = Lerp(elev, trenchDepth, c.tectonicStress * 0.8);
            }

            // Foreland basin: compression depression in front of collision belt
            if (c.surfaceRock == RockType::SedimentaryBasin && c.tectonicStress > 0.2)
                elev -= c.tectonicStress * 1.2;

            // Local fBm detail (±1.5 km): unresolved faulting and local geology
            const double detail = fBmGrid(la, lo, latB, lonS, seed, 4, 8, 6);
            elev += (detail - 0.5) * 3.0;

            elevKm[idx] = elev;
        }
    }

    // -----------------------------------------------------------------------
    // Pass 2: erosion smoothing — older planets have softer relief
    // -----------------------------------------------------------------------
    const double erosionStrength = ageFrac * 0.45 * gravScale;

    if (erosionStrength > 0.02)
    {
        std::vector<double> smoothed = elevKm;
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                const double e = elevKm[idx];
                if (e < 0.0) continue;

                double sum = 0.0; int count = 0;
                for (int dla = -1; dla <= 1; ++dla)
                    for (int dlo = -1; dlo <= 1; ++dlo)
                    {
                        const int nla = std::clamp(la + dla, 0, latB-1);
                        const int nlo = ((lo + dlo) + lonS) % lonS;
                        sum += elevKm[static_cast<std::size_t>(nla)*lonS + nlo];
                        ++count;
                    }
                const double avg = sum / count;
                const double eFactor = erosionStrength * std::clamp(e / 5.0, 0.0, 1.0);
                smoothed[idx] = Lerp(e, avg, eFactor);
            }
        }
        elevKm = smoothed;
    }

    // -----------------------------------------------------------------------
    // Pass 3: normalise to [-1..1]
    // -----------------------------------------------------------------------
    constexpr double kMaxElevKm = 9.0;

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const double ek = elevKm[static_cast<std::size_t>(la)*lonS + lo];
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
            const int la1 = std::clamp(la-1, 0, latB-1);
            const int la2 = std::clamp(la+1, 0, latB-1);
            const int lo1 = ((lo-1) + lonS) % lonS;
            const int lo2 = ((lo+1) + lonS) % lonS;
            constexpr double kDkm = kMaxElevKm * 2.0;
            const double dLat = (Cell(la2,lo).elevationNormalized - Cell(la1,lo).elevationNormalized) * kDkm;
            const double dLon = (Cell(la,lo2).elevationNormalized - Cell(la,lo1).elevationNormalized) * kDkm;
            c.slopeNormalized = std::clamp(std::hypot(dLat, dLon) / (2.0 * 200.0), 0.0, 1.0);
        }
    }
}

// ==========================================================================
// Stage 3 — Climate (moved before Hydrology)
//
// Key change: EquilibriumTempK now uses 255 K (Earth's effective emission
// temperature without greenhouse) instead of 288 K (surface temperature).
// Greenhouse warming is added separately via GreenhouseK(), so the two
// effects no longer double-count each other.
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

    // Global equilibrium temperature and greenhouse delta:
    //   255 K × flux^0.25 → emission temperature (no greenhouse)
    //   + GreenhouseK()   → surface warming from atmosphere
    // Earth: 255 × 1^0.25 + 33 = 288 K → 15°C average ✓
    const double globalEquilK = EquilibriumTempK(flux);
    const double greenhouse   = GreenhouseK(pressKPa, volFrac);
    const double pressureScale = std::clamp(pressKPa / 101.325, 0.1, 4.0);

    // Pass 1: temperature per cell
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);

            const double insolF = InsolationFraction(c.latitudeDegrees, tilt);
            // Latitude temperature: scale from global equilibrium by insolation.
            // The sub-solar latitude gets ~1.4× the global mean; poles get near 0.
            const double latTempK = (globalEquilK - 240.0) * (insolF * 1.5 + 0.04)
                                    + 240.0 + greenhouse;
            // Ocean thermal buffering: maritime climates have smaller swings
            const double thermalBuf = c.isOcean ? 0.70 : 1.0;
            const double bufferedK  = (latTempK - 250.0) * thermalBuf + 250.0;
            // Altitude lapse cooling (6.5 K/km, gravity-adjusted)
            const double lapseK_ = LapseK(c.elevationNormalized, grav);
            // Local temperature noise ±4°C (unresolved weather / micro-climate)
            const double noisePert =
                (fBmGrid(la, lo, latB, lonS, seed ^ 0xaa55cc77, 5, 10, 4) - 0.5) * 8.0;

            c.temperatureCelsius = bufferedK - 273.15 - lapseK_ + noisePert;

            // Seasonal amplitude: ocean moderates, land amplifies
            c.temperatureAnnualRangeC =
                SeasonalAmplitude(c.latitudeDegrees, tilt, rot) * (c.isOcean ? 0.4 : 1.0);

            c.continentality = 0.0; // accumulated in Pass 2
        }
    }

    // Pass 2: continentality (fraction of land in ~10° neighbourhood)
    {
        const int windowLat = std::max(1, latB / 18);
        const int windowLon = std::max(1, lonS / 36);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                int land = 0, total = 0;
                for (int dla = -windowLat; dla <= windowLat; ++dla)
                {
                    for (int dlo = -windowLon; dlo <= windowLon; ++dlo)
                    {
                        const int nla = std::clamp(la + dla, 0, latB-1);
                        const int nlo = ((lo + dlo) + lonS) % lonS;
                        if (!Cell(nla, nlo).isOcean) ++land;
                        ++total;
                    }
                }
                Cell(la, lo).continentality = total > 0
                    ? static_cast<double>(land) / total : 0.0;
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

            const double circW     = CirculationWetness(absLat, rot);
            const double moistBase = std::clamp(volFrac / 0.02, 0.1, 5.0);
            const double contDry   = 1.0 - c.continentality * 0.6;
            const double moistNoise =
                fBmGrid(la, lo, latB, lonS, seed ^ 0x1234abcd, 5, 10, 5);
            const double orographic = c.isOcean ? 0.0
                : std::max(0.0, c.slopeNormalized * 0.5 - c.tectonicStress * 0.1);

            c.humidityNormalized = std::clamp(
                circW * contDry * moistBase * (0.5 + moistNoise*0.5) * (1.0 + orographic),
                0.0, 1.0);

            // Clausius-Clapeyron: cold air holds less moisture
            const double satFactor =
                std::clamp(std::exp((c.temperatureCelsius) * 0.065) / 10.0, 0.01, 2.0);

            c.precipitationMm = std::max(0.0,
                c.humidityNormalized * 1400.0 * pressureScale * satFactor);

            // Rain shadow on lee side of mountains
            if (!c.isOcean && c.slopeNormalized > 0.3 && circW < 0.45)
                c.precipitationMm *= std::clamp(1.0 - c.slopeNormalized * 0.7, 0.2, 1.0);

            // Soil moisture: precip minus crude potential evapotranspiration
            const double pet = std::max(0.0, c.temperatureCelsius * 15.0);
            c.soilMoisture = std::clamp(
                c.precipitationMm / (c.precipitationMm + pet + 1.0), 0.0, 1.0);

            c.drySeasonMonths = DrySeasonMonths(c.precipitationMm, circW);

            const double snowTemp = std::clamp((-c.temperatureCelsius) / 10.0, 0.0, 1.0);
            c.snowDays = snowTemp * std::clamp(c.precipitationMm / 800.0, 0.0, 1.0) * 200.0;
        }
    }
}

// ==========================================================================
// Stage 4 — Hydrology (moved after Climate)
//
// Key change: rivers now require a precipitation-scaled water budget.
// drainageAccumulation still uses the topographic D8 algorithm, but
// hasRiver additionally requires precipitationMm to be sufficient.
// This prevents rivers in dry deserts with purely topographic drainage.
// ==========================================================================
void PlanetMap::GenerateHydrology()
{
    const int latB = LatBands();
    const int lonS = LonSegs();

    std::vector<int> accumulation(static_cast<std::size_t>(latB)*lonS, 0);
    std::vector<int> flowDir(static_cast<std::size_t>(latB)*lonS, -1);

    const int dx[8] = { -1, -1, -1,  0,  0,  1,  1,  1 };
    const int dy[8] = { -1,  0,  1, -1,  1, -1,  0,  1 };

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const auto& c = Cell(la, lo);
            if (c.isOcean) continue;

            double minElev = c.elevationNormalized;
            int bestNeighbor = -1;
            for (int d = 0; d < 8; ++d)
            {
                const int nla = la + dx[d];
                const int nlo = ((lo + dy[d]) + lonS) % lonS;
                if (nla < 0 || nla >= latB) continue;
                const double ne = Cell(nla, nlo).elevationNormalized;
                if (ne < minElev) { minElev = ne; bestNeighbor = nla*lonS + nlo; }
            }
            flowDir[static_cast<std::size_t>(la)*lonS + lo] = bestNeighbor;
        }
    }

    std::vector<int> order(static_cast<std::size_t>(latB)*lonS);
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

    const int maxAcc = *std::max_element(accumulation.begin(), accumulation.end());

    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            auto& c = Cell(la, lo);
            const std::size_t idx = static_cast<std::size_t>(la)*lonS + lo;
            const int acc = accumulation[idx];
            c.drainageAccumulation = maxAcc > 0
                ? static_cast<double>(acc) / maxAcc : 0.0;

            // River threshold: topographic accumulation AND water budget.
            // Normalise precipitation to Earth-like baseline (400 mm/yr = arid but
            // sufficient for perennial rivers). Hyper-arid cells (< 50 mm/yr) cannot
            // sustain rivers regardless of topographic accumulation.
            constexpr double kDrainageThreshold = 0.03;
            constexpr double kMinPrecipForRiver = 50.0; // mm/yr
            const double waterBudget =
                std::clamp(c.precipitationMm / 400.0, 0.0, 1.0);

            c.hasRiver = !c.isOcean
                && c.precipitationMm >= kMinPrecipForRiver
                && c.drainageAccumulation * waterBudget > kDrainageThreshold;
            c.riverDischarge = c.hasRiver ? c.drainageAccumulation * waterBudget : 0.0;

            // Endorheic lake: no downslope outlet, sufficient catchment, not ocean
            c.isLake = !c.isOcean
                && flowDir[idx] == -1
                && c.drainageAccumulation > 0.005;
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
            if (c.isOcean) { c.soilFertility = 0.0; c.soilDepthNorm = 0.0; continue; }

            c.soilFertility = SoilFertility(
                c.surfaceRock, c.temperatureCelsius, c.precipitationMm, ageFrac);
            c.soilDepthNorm = SoilDepth(
                c.surfaceRock, c.precipitationMm, c.slopeNormalized, ageFrac);

            // River alluvium: flat valley bottom → very fertile, deep soil
            if (c.hasRiver && c.slopeNormalized < 0.2)
            {
                c.soilFertility = std::min(1.0, c.soilFertility * 1.5);
                c.soilDepthNorm = std::min(1.0, c.soilDepthNorm  * 1.8);
            }

            // Volcanic terrain: initially barren, highly fertile after weathering
            if (c.isVolcanicZone)
            {
                const double weatheredFrac = std::clamp(ageFrac * 2.0, 0.0, 1.0);
                c.soilFertility = Lerp(0.1, 0.85, weatheredFrac);
                c.soilDepthNorm = Lerp(0.1, 0.6,  weatheredFrac);
            }

            // Permafrost: thin, infertile soil
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
