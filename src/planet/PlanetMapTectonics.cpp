#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <queue>
#include <utility>
#include <vector>

namespace Infinitesia
{
    using namespace GenerationDetail;

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
    const double ageFrac   = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);
    // Older planets lose internal heat → fewer, larger, slower plates
    const double heatScale = std::sqrt(massFrac) * (1.0 - 0.4 * ageFrac);
    const int numPlates    = static_cast<int>(std::clamp(5.0 + heatScale * 11.0, 5.0, 16.0));

    // -----------------------------------------------------------------------
    // Continental lithosphere is spatially continuous and independent of plate
    // boundaries. The 40% target approximates Earth's continental-crust area;
    // it is a calibration, not a prediction from bulk composition.
    // -----------------------------------------------------------------------
    constexpr double kContinentalCrustAreaFraction = 0.40;
    const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
    std::vector<double> continentalField(cellCount);
    std::vector<std::pair<double, double>> areaRankedField;
    areaRankedField.reserve(cellCount);
    double totalAreaWeight = 0.0;
    for (int la = 0; la < latB; ++la)
    {
        for (int lo = 0; lo < lonS; ++lo)
        {
            const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
            const PlanetRegion& cell = Cell(la, lo);
            const double field = SphericalFbm(
                cell.latitudeDegrees, cell.longitudeDegrees, seed ^ 0x71a1d001u, 3, 5);
            const double areaWeight =
                std::max(0.0, std::cos(cell.latitudeDegrees * std::numbers::pi / 180.0));
            continentalField[index] = field;
            areaRankedField.emplace_back(field, areaWeight);
            totalAreaWeight += areaWeight;
        }
    }

    std::sort(areaRankedField.begin(), areaRankedField.end(),
        [](const auto& first, const auto& second) { return first.first < second.first; });
    const double targetOceanArea =
        totalAreaWeight * (1.0 - kContinentalCrustAreaFraction);
    double rankedArea = 0.0;
    double continentalThreshold = 0.5;
    for (const auto& [field, areaWeight] : areaRankedField)
    {
        rankedArea += areaWeight;
        continentalThreshold = field;
        if (rankedArea >= targetOceanArea)
            break;
    }
    std::vector<bool> isContinentalCrust(cellCount, false);
    for (std::size_t index = 0; index < cellCount; ++index)
        isContinentalCrust[index] = continentalField[index] > continentalThreshold;

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

        plates[p].isOceanic =
            SphericalFbm(plates[p].centerLat, plates[p].centerLon,
                         seed ^ 0x71a1d001u, 3, 5) <= continentalThreshold;

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
        // Reference angular rates of 0.09–0.9 deg/Myr correspond to
        // approximately 1–10 cm/yr at Earth's radius before heat scaling.
        // In Euler-pole terms: ω = v_equatorial / R_equatorial.
        // For a unit sphere, R=1 rad → ω_rad/Myr ≈ v_deg/Myr × π/180.
        //
        // Apply the mass/age heatScale, then convert deg/Myr to rad/Myr.
        // -----------------------------------------------------------------------
        const double speedDegPerMy = RandRange(seed, pu*7u+5u, 0.09, 0.9) * heatScale;

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
            // Multi-source Dijkstra finds distance to the nearest oceanic ridge
            // across the spherical grid in O(N log N), rather than comparing each
            // ocean cell with every cell in the grid.
            constexpr double kMaxAgeDistDeg = 80.0;
            const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
            std::vector<double> distance(cellCount, std::numeric_limits<double>::infinity());
            using QueueEntry = std::pair<double, int>;
            std::priority_queue<
                QueueEntry,
                std::vector<QueueEntry>,
                std::greater<QueueEntry>> frontier;

            for (const int ridgeIndex : morCells)
            {
                distance[static_cast<std::size_t>(ridgeIndex)] = 0.0;
                frontier.emplace(0.0, ridgeIndex);
            }

            constexpr int dLat[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
            constexpr int dLon[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
            const double latitudeStep = 180.0 / latB;
            const double longitudeStep = 360.0 / lonS;

            while (!frontier.empty())
            {
                const auto [distanceToCell, index] = frontier.top();
                frontier.pop();
                if (distanceToCell != distance[static_cast<std::size_t>(index)])
                    continue;

                const int la = index / lonS;
                const int lo = index % lonS;
                const double latitude =
                    (static_cast<double>(la) + 0.5) * latitudeStep - 90.0;

                for (int direction = 0; direction < 8; ++direction)
                {
                    const int nextLatitude = la + dLat[direction];
                    if (nextLatitude < 0 || nextLatitude >= latB)
                        continue;
                    const int nextLongitude =
                        (lo + dLon[direction] + lonS) % lonS;
                    const std::size_t nextIndex =
                        static_cast<std::size_t>(nextLatitude) * lonS + nextLongitude;

                    // Ocean-crust age is only meaningful within the oceanic domain.
                    // Blocking propagation through continental cells prevents
                    // Dijkstra from routing a "short-cut" path across a continent
                    // and giving the oceanic crust on the far side an artificially
                    // young age that does not reflect its actual spreading history.
                    if (isContinentalCrust[nextIndex])
                        continue;

                    const double nextLatitudeDegrees =
                        (static_cast<double>(nextLatitude) + 0.5) * latitudeStep - 90.0;
                    const double meanLatitudeRadians =
                        (latitude + nextLatitudeDegrees) * 0.5 *
                        std::numbers::pi / 180.0;
                    const double northwardDistance =
                        static_cast<double>(dLat[direction]) * latitudeStep;
                    const double eastwardDistance =
                        static_cast<double>(dLon[direction]) * longitudeStep *
                        std::cos(meanLatitudeRadians);
                    const double edgeDistance =
                        std::hypot(northwardDistance, eastwardDistance);
                    const double candidateDistance = distanceToCell + edgeDistance;
                    if (candidateDistance < distance[nextIndex])
                    {
                        distance[nextIndex] = candidateDistance;
                        frontier.emplace(candidateDistance, nextLatitude * lonS + nextLongitude);
                    }
                }
            }

            for (int la = 0; la < latB; ++la)
            {
                for (int lo = 0; lo < lonS; ++lo)
                {
                    const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
                    if (isContinentalCrust[index])
                        continue;

                    Cell(la, lo).crustAge =
                        std::clamp(distance[index] / kMaxAgeDistDeg, 0.0, 1.0);
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
                    if (!isContinentalCrust[static_cast<std::size_t>(la) * lonS + lo])
                        c.crustAge = std::clamp(a.bestDist / 90.0, 0.0, 1.0);
                }
            }
        }

    // Boundary subtype can change abruptly between adjacent cells even when
    // both lie on the same fault. Blur the signed relief field so those
    // classification changes become broad mountain belts and trenches.
    std::vector<double> relief(
        static_cast<std::size_t>(latB) * static_cast<std::size_t>(lonS));
    for (int la = 0; la < latB; ++la)
        for (int lo = 0; lo < lonS; ++lo)
            relief[static_cast<std::size_t>(la) * lonS + lo] =
                Cell(la, lo).tectonicReliefKm;

    for (int pass = 0; pass < 5; ++pass)
    {
        std::vector<double> smoothed(relief.size());
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                double neighborhoodSum =
                    relief[static_cast<std::size_t>(la) * lonS + lo] * 4.0;
                double neighborhoodWeight = 4.0;
                for (int dla = -1; dla <= 1; ++dla)
                {
                    for (int dlo = -1; dlo <= 1; ++dlo)
                    {
                        if (dla == 0 && dlo == 0)
                            continue;
                        const int neighborLatitude =
                            std::clamp(la + dla, 0, latB - 1);
                        const int neighborLongitude =
                            (lo + dlo + lonS) % lonS;
                        neighborhoodSum += relief[
                            static_cast<std::size_t>(neighborLatitude) * lonS +
                            neighborLongitude];
                        neighborhoodWeight += 1.0;
                    }
                }
                const std::size_t index =
                    static_cast<std::size_t>(la) * lonS + lo;
                smoothed[index] = Lerp(
                    relief[index],
                    neighborhoodSum / neighborhoodWeight,
                    0.55);
            }
        }

        relief = std::move(smoothed);
    }

    for (int la = 0; la < latB; ++la)
        for (int lo = 0; lo < lonS; ++lo)
            Cell(la, lo).tectonicReliefKm =
                relief[static_cast<std::size_t>(la) * lonS + lo];
    }
}

} // namespace Infinitesia
