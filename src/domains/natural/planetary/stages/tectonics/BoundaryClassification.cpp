#include "domains/natural/planetary/stages/tectonics/TectonicsInternal.h"

#include "domains/natural/planetary/internal/GenerationMath.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Infinitesia::TectonicsDetail
{
    using namespace GenerationDetail;

    namespace
    {
        PlateBoundaryKind ToPlateBoundaryKind(BoundarySubtype subtype)
        {
            switch (subtype)
            {
            case BoundarySubtype::MidOceanRidge: return PlateBoundaryKind::MidOceanRidge;
            case BoundarySubtype::ContinentalRift: return PlateBoundaryKind::ContinentalRift;
            case BoundarySubtype::OceanicSubduction: return PlateBoundaryKind::OceanicSubduction;
            case BoundarySubtype::ContinentalSubduction: return PlateBoundaryKind::ContinentalSubduction;
            case BoundarySubtype::ContinentalCollision: return PlateBoundaryKind::ContinentalCollision;
            case BoundarySubtype::TransformFault: return PlateBoundaryKind::TransformFault;
            }
            return PlateBoundaryKind::None;
        }
    }

    std::vector<bool> ClassifyBoundaries(
        PlanetBuild& build, std::uint32_t seed,
        const std::vector<TectonicPlate>& plates,
        const std::vector<CellAssignment>& assignments, const CrustField& crust)
    {
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();
        const std::vector<double>& continentalField = crust.field;
        const std::vector<bool>& isContinentalCrust = crust.isContinental;
        const double continentalThreshold = crust.threshold;

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
                auto& c = build.Cell(la, lo);
                const auto& a  = assignments[static_cast<std::size_t>(la)*lonS + lo];
                const TectonicPlate& plateA = plates[a.best];
                const TectonicPlate& plateB = plates[a.second];

                c.tectonicPlateId = a.best;

                // Keep the diagnostic stress narrow. Geological relief uses a
                // wider envelope below, so the boundary is expressed as terrain
                // rather than as a thin painted line.
                constexpr double kBoundaryWidth = 1.6;
                const double boundaryGap = a.secondDist - a.bestDist;
                c.tectonicStress = SmootherStep(
                    std::clamp(1.0 - boundaryGap / kBoundaryWidth, 0.0, 1.0));
                const double broadBoundaryInfluence = SmootherStep(
                    std::clamp(1.0 - boundaryGap / 4.5, 0.0, 1.0));

                // Boundary normal in lat/lon tangent space (approximate)
                const double dLat = plateB.centerLat - plateA.centerLat;
                double dLon = std::remainder(plateB.centerLon - plateA.centerLon, 360.0) *
                              std::cos(c.latitudeDegrees * std::numbers::pi / 180.0);
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

                const std::size_t cellIndex = static_cast<std::size_t>(la) * lonS + lo;
                const bool continentalCell = isContinentalCrust[cellIndex];

                // Determine the local crust character on each side of the boundary
                // using the per-cell fBm field, not the plate-centre isOceanic flag.
                //
                // Side A: the current cell itself.
                const bool cellAOceanic = continentalField[cellIndex] <= continentalThreshold;

                // Side B: find the nearest 8-connected neighbour assigned to plateB
                // and read its fBm field value.  This gives a local reading of what
                // the crust on the other side of this specific boundary segment
                // actually is, even when plateB's centre is far away and of a
                // different character than its margin.
                //
                // Fall back to plateB.isOceanic if no B-side neighbour is found
                // (e.g., isolated single-cell plates at coarse resolutions).
                constexpr int kNbrDLa[8] = { -1, -1, -1, 0, 0,  1, 1, 1 };
                constexpr int kNbrDLo[8] = { -1,  0,  1,-1, 1, -1, 0, 1 };
                bool cellBOceanic = plateB.isOceanic;
                for (int nd = 0; nd < 8; ++nd)
                {
                    const int nla = la + kNbrDLa[nd];
                    if (nla < 0 || nla >= latB)
                        continue;
                    const int nloW = (lo + kNbrDLo[nd] + lonS) % lonS;
                    const std::size_t nIdx = static_cast<std::size_t>(nla) * lonS + nloW;
                    if (assignments[nIdx].best == a.second)
                    {
                        cellBOceanic = continentalField[nIdx] <= continentalThreshold;
                        break;
                    }
                }

                const BoundarySubtype bst = DetailedBoundary(bt, cellAOceanic, cellBOceanic);

                // Geological context of the cell: nearest boundary, plate motion. These
                // are recorded for later stages and feed nothing below.
                {
                    const double radiusKm = build.data.planet.GetRadius() / 1000.0;
                    const double kmPerDegree = radiusKm * std::numbers::pi / 180.0;
                    // 1 deg/Myr of arc is radiusKm*1e5*pi/180 cm per 1e6 yr.
                    const double cmPerYearPerDegPerMy = kmPerDegree * 1.0e5 / 1.0e6;

                    c.distanceToBoundaryKm = 0.5 * boundaryGap * kmPerDegree;
                    c.boundarySubtype = c.distanceToBoundaryKm <= kBoundaryReachKm
                        ? ToPlateBoundaryKind(bst)
                        : PlateBoundaryKind::None;
                    switch (bst)
                    {
                    case BoundarySubtype::ContinentalSubduction:
                        c.isSubductingPlate = cellAOceanic;
                        break;
                    case BoundarySubtype::OceanicSubduction:
                        // The denser (older, colder) oceanic plate goes down.
                        c.isSubductingPlate = plateA.crustDensity > plateB.crustDensity ||
                            (plateA.crustDensity == plateB.crustDensity && plateA.id > plateB.id);
                        break;
                    default:
                        c.isSubductingPlate = false;
                        break;
                    }
                    c.plateVelocityEastCmPerYear = vLonA * cmPerYearPerDegPerMy;
                    c.plateVelocityNorthCmPerYear = vLatA * cmPerYearPerDegPerMy;
                    c.boundaryRelativeVelocityEastCmPerYear = (vLonA - vLonB) * cmPerYearPerDegPerMy;
                    c.boundaryRelativeVelocityNorthCmPerYear = (vLatA - vLatB) * cmPerYearPerDegPerMy;
                }

                const double beltStrength =
                    broadBoundaryInfluence * broadBoundaryInfluence;
                if (bt == BoundaryType::Convergent)
                {
                    if (continentalCell)
                        c.tectonicReliefKm = 4.5 * beltStrength;
                    else
                        c.tectonicReliefKm = -3.2 * beltStrength;
                }
                else if (bt == BoundaryType::Divergent)
                {
                    c.tectonicReliefKm = continentalCell
                        ? -0.8 * beltStrength
                        : 1.2 * beltStrength;
                }
                else
                {
                    c.tectonicReliefKm = 0.25 * beltStrength;
                }

                const bool oceanicSpreadingBoundary =
                    c.tectonicStress > 0.85 &&
                    bt == BoundaryType::Divergent &&
                    !continentalCell;
                if (oceanicSpreadingBoundary)
                    isDivergentBoundary[cellIndex] = true;

                // ----------------------------------------------------------------
                // Interior crust properties
                // ----------------------------------------------------------------
                const double crustVariation =
                    fBmGrid(la, lo, latB, lonS, seed ^ 0x6a09e667u, 3, 6, 4);
                const double oceanicThickness = 5.0 + crustVariation * 5.0;
                const double continentalThickness = 30.0 + crustVariation * 12.0;
                const double marginWidth = 0.10;
                c.continentalCrustFraction = SmootherStep(
                    0.5 + (continentalField[cellIndex] - continentalThreshold) /
                              (2.0 * marginWidth));
                c.crustThickness = oceanicThickness +
                    (continentalThickness - oceanicThickness) *
                        c.continentalCrustFraction;
                c.isostaticCrustThicknessKm = c.crustThickness;
                if (!isContinentalCrust[cellIndex])
                {
                    c.crustAge = RandRange(seed,
                        static_cast<std::uint32_t>(la*lonS + lo) ^ 0xbeef0000u, 0.0, 1.0);
                    c.surfaceRock    = RockType::BasalticOceanic;
                    c.isVolcanicZone = false;
                }
                else
                {
                    // Continental interior age via low-frequency fBm: coherent regions
                    // (old cratons vs younger basins), not random per cell
                    const double ageSample = SphericalFbm(
                        c.latitudeDegrees, c.longitudeDegrees, seed ^ 0xcafe0000u, 3, 4);
                    c.crustAge = ageSample; // [0..1], spatially coherent
                    c.surfaceRock    = (ageSample > 0.65)
                        ? RockType::GraniticShield    // old craton
                        : RockType::SedimentaryBasin; // younger basin or platform
                    c.isVolcanicZone = false;
                }

                // ----------------------------------------------------------------
                // Boundary-zone overrides (applied proportional to stress)
                // ----------------------------------------------------------------
                if (c.tectonicStress > 0.85)
                {
                    // Relative speed at this point (for thickening scale)
                    const double relSpeed = std::hypot(vLatA - vLatB, vLonA - vLonB);

                    switch (bst)
                    {
                    case BoundarySubtype::ContinentalCollision:
                    {
                        // Broad collision uplift is represented by tectonicReliefKm;
                        // keep the narrow core's crustal modification modest.
                        c.crustThickness +=
                            std::min(c.tectonicStress * std::max(relSpeed, 0.1), 2.0);
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
                        c.crustThickness = 7.0;
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
                if (!isContinentalCrust[cellIndex] && c.tectonicStress < 0.1 &&
                    RandUnit(seed ^ 0xbabe, la, lo) < 0.006)
                {
                    c.isVolcanicZone = true;
                    c.surfaceRock    = RockType::VolcanicIgneous;
                    c.crustThickness = 18.0;
                }
            }
        }

        return isDivergentBoundary;
    }
}
