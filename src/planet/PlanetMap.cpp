// PlanetMap.cpp
// PlanetMap construction and shared deterministic generation helpers.

#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia
{

// ==========================================================================
// Shared internal generation helpers
// ==========================================================================
namespace GenerationDetail
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
Vec3d LatLonToCart(double latDeg, double lonDeg)
{
    const double lat = latDeg * std::numbers::pi / 180.0;
    const double lon = lonDeg * std::numbers::pi / 180.0;
    return { std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon) };
}

double SphericalFbm(
    double latitudeDegrees,
    double longitudeDegrees,
    std::uint32_t seed,
    int baseCells,
    int octaves)
{
    const Vec3d point = LatLonToCart(latitudeDegrees, longitudeDegrees);
    double value = 0.0;
    double amplitude = 1.0;
    double amplitudeSum = 0.0;
    int cells = baseCells;

    for (int octave = 0; octave < octaves; ++octave)
    {
        const double x = (point.x + 1.0) * 0.5 * cells;
        const double y = (point.y + 1.0) * 0.5 * cells;
        const double z = (point.z + 1.0) * 0.5 * cells;
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int z0 = static_cast<int>(std::floor(z));
        const double tx = SmootherStep(x - x0);
        const double ty = SmootherStep(y - y0);
        const double tz = SmootherStep(z - z0);

        const auto sample = [seed](int ix, int iy, int iz)
        {
            std::uint32_t hash = seed;
            hash ^= static_cast<std::uint32_t>(ix + 1) * kHashMul1;
            hash ^= static_cast<std::uint32_t>(iy + 1) * kHashMul2;
            hash ^= Hash32(static_cast<std::uint32_t>(iz + 1) * 0xc2b2ae35u);
            return static_cast<double>(Hash32(hash)) /
                   static_cast<double>(0xffffffffu);
        };
        const auto interpolate = [&sample, tx, ty, tz, x0, y0, z0]()
        {
            const double x00 = Lerp(sample(x0, y0, z0), sample(x0 + 1, y0, z0), tx);
            const double x10 = Lerp(sample(x0, y0 + 1, z0), sample(x0 + 1, y0 + 1, z0), tx);
            const double x01 = Lerp(sample(x0, y0, z0 + 1), sample(x0 + 1, y0, z0 + 1), tx);
            const double x11 = Lerp(sample(x0, y0 + 1, z0 + 1), sample(x0 + 1, y0 + 1, z0 + 1), tx);
            return Lerp(Lerp(x00, x10, ty), Lerp(x01, x11, ty), tz);
        };

        value += interpolate() * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5;
        cells *= 2;
        seed = Hash32(seed ^ 0xdeadbeefu);
    }

    return amplitudeSum > 0.0 ? value / amplitudeSum : 0.0;
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
// --------------------------------------------------------------------------
// Boundary classification using Euler-pole velocities
//
// The boundary normal is the unit vector pointing from plate A's centre toward
// plate B's centre (A→B direction in lat/lon tangent space).
//
// dvLat/dvLon = vA − vB is the velocity of plate A in plate B's reference frame.
//
// normalComponent = (vA−vB) · normal:
//   positive → A advances in the A→B direction → plates closing → Convergent
//   negative → A retreats in the A→B direction → plates opening → Divergent
//
// tangential / normal ratio distinguishes Transform from Convergent/Divergent.
//
// Thresholds scaled to actual velocity magnitudes (~0.01–0.1 deg/Myr).
// A boundary is Convergent/Divergent when the normal component exceeds
// a threshold AND dominates over the tangential component.
// --------------------------------------------------------------------------
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
    //   positive → A moves toward  B  (Convergent)
    //   negative → A moves away from B (Divergent)
    //
    // Derivation: dvLat/dvLon = vA - vB is the velocity of A in B's reference
    // frame.  The unit normal points from A's centre toward B's centre (A→B).
    // When dvA-dvB · normal > 0, plate A is advancing in the A→B direction,
    // i.e. the two plates are closing → Convergent.
    // When dvA-dvB · normal < 0, plate A is retreating from B → Divergent.
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

    if (normalComponent > threshold)
        return BoundaryType::Convergent;
    if (normalComponent < -threshold)
        return BoundaryType::Divergent;

    return BoundaryType::Transform;
}

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
const double kOceanicRho = 2900.0;
const double kContinentalRho = 2750.0;
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
    const double latitude = latDeg * std::numbers::pi / 180.0;
    const double tilt = tiltDeg * std::numbers::pi / 180.0;
    const double sinLatitude = std::sin(latitude);
    const double cosLatitude = std::cos(latitude);
    double seasonalMean = 0.0;
    constexpr int kSeasonSamples = 64;

    for (int sample = 0; sample < kSeasonSamples; ++sample)
    {
        const double orbitalLongitude =
            2.0 * std::numbers::pi *
            (static_cast<double>(sample) + 0.5) / kSeasonSamples;
        const double declination =
            std::asin(std::sin(tilt) * std::sin(orbitalLongitude));
        const double hourAngleArgument =
            -std::tan(latitude) * std::tan(declination);
        const double sunsetHourAngle = hourAngleArgument <= -1.0
            ? std::numbers::pi
            : hourAngleArgument >= 1.0
                ? 0.0
                : std::acos(hourAngleArgument);
        const double dailyMean =
            sunsetHourAngle * sinLatitude * std::sin(declination) +
            cosLatitude * std::cos(declination) *
                std::sin(sunsetHourAngle);
        seasonalMean += dailyMean;
    }

    // Normalize by the spherical global mean (one quarter of the solar constant).
    return std::max(
        0.0,
        (4.0 / std::numbers::pi) * seasonalMean / kSeasonSamples);
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
// Stefan-Boltzmann for a planet with a given Bond albedo:
//   T_eff = 278.5 K × (1 - A)^0.25 × flux^0.25
//
// 278.5 K = (S₀ / 4σ)^0.25 where S₀ = 1361 W/m² and σ = 5.67e-8 W/m²/K⁴.
// For Earth (A = 0.30, flux = 1.0) this gives 255 K, matching observations.
//
// The Bond albedo is kept at the Earth-like default of 0.30 here.
// A future improvement would vary it with ice coverage and cloud fraction
// derived from the climate system.  For now, the fixed value is documented
// as a known approximation (see ClimateInputs).
double EquilibriumTempK(double flux)
{
    // Bond albedo A = 0.30 (Earth-like; fixed approximation — see comment above).
    constexpr double kBondAlbedo = 0.30;
    // 278.5 K = (S₀ / 4σ)^0.25
    constexpr double kStefanBase = 278.5;
    return kStefanBase *
        std::pow(1.0 - kBondAlbedo, 0.25) *
        std::pow(std::max(0.01, flux), 0.25);
}

// Greenhouse warming from atmospheric pressure.
//
// APPROXIMATION: this function models the greenhouse effect as a function of
// total surface pressure alone, calibrated to reproduce Earth's ~33 K warming
// at 101.325 kPa.  It does NOT account for:
//   • Atmospheric composition (CO₂ fraction, CH₄, H₂O vapour pressure).
//   • Spectroscopic line strengths of individual greenhouse gases.
//   • volatileMassFraction from ClimateInputs (stored but currently unused).
//
// The √(pressure) scaling is a rough proxy; real greenhouse sensitivity is
// logarithmic in CO₂ concentration.  This is acceptable for an Earth-like
// default scenario but will be inaccurate for high-CO₂ or pure-N₂ atmospheres.
// A future improvement: split the contribution into a CO₂ term (logarithmic)
// and a water-vapour feedback term (temperature-dependent).
double GreenhouseK(double pressKPa)
{
    if (pressKPa <= 0.0) return 0.0;
    return 33.0 * std::sqrt(pressKPa / 101.325);
}

// Altitude lapse cooling [K]
double LapseK(double elevNorm)
{
    const double elevM = std::max(0.0, elevNorm) * 9000.0;
    return elevM * 0.0065;
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
// Biome classification — Köppen-extended, 25 types
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
        if (p > 1500.0 && dry < 2.0) return Biome::TemperateRainforest;
        if (p > 700.0) return Biome::TemperateDeciduous;
        if (p > 400.0) return Biome::TemperateMixed;
        if (p > 200.0) return Biome::TemperateSteppe;
        return Biome::ColdDesert;
    }

    // Hot desert
    if (p < 200.0) return Biome::HotDesert;

    // Subtropical / tropical
    if (dry >= 2.0 && dry <= 7.0 && p > 1100.0)
        return Biome::TropicalMonsoonForest;
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

} // namespace GenerationDetail

using namespace GenerationDetail;

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

    // Climate precedes hydrology so river generation can use precipitation.
    GenerateTectonics();
    GenerateTerrain();
    GenerateClimate();
    GenerateHydrology();
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

} // namespace Infinitesia
