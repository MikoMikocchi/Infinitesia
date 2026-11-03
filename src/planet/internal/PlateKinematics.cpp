#include "planet/internal/PlateKinematics.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia::GenerationDetail
{

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

}
