#include "planet/stages/tectonics/TectonicsInternal.h"

#include "planet/internal/GenerationMath.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <queue>
#include <utility>

namespace Infinitesia::TectonicsDetail
{
    using namespace GenerationDetail;

    namespace
    {
        // Oceanic crust age is stored normalized over this many million years.
        constexpr double kOceanicCrustAgeScaleMyr = 180.0;
    }

    void AssignOceanicCrustAge(
        PlanetBuild& build, const std::vector<CellAssignment>& assignments,
        const CrustField& crust, const std::vector<bool>& isDivergentBoundary)
    {
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();
        const std::vector<bool>& isContinentalCrust = crust.isContinental;

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

                    build.Cell(la, lo).crustAge =
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
                    auto& c = build.Cell(la, lo);
                    const auto& a = assignments[static_cast<std::size_t>(la)*lonS + lo];
                    if (!isContinentalCrust[static_cast<std::size_t>(la) * lonS + lo])
                        c.crustAge = std::clamp(a.bestDist / 90.0, 0.0, 1.0);
                }
            }
        }
    }

    void AssignAbsoluteCrustAge(PlanetBuild& build, const CrustField& crust)
    {
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();
        const std::vector<bool>& isContinentalCrust = crust.isContinental;

        // Absolute crust age. Oceanic crust keeps the normalization its bathymetry uses.
        // Continental crust keeps the spatial pattern of its coherent age field (so cratons
        // stay where the field is high), but the ages are assigned by rank: the oldest
        // quarter of the continental area is older than two thirds of the planet's age (the
        // cratonic cores), the rest spreads over younger crust. Without this the share of
        // old crust would follow the noise field's mean, which varies from seed to seed.
        // The craton share is a gameplay calibration.
        constexpr double kCratonAreaShare = 0.28;
        constexpr double kCratonAgeFraction = 2.0 / 3.0;
        constexpr double kYoungestContinentalAgeFraction = 0.05;
        const double planetAgeMyr = std::max(build.data.climate.planetAgeYears / 1.0e6, 1.0);

        struct RankedCell
        {
            double age;
            std::size_t index;
            double area;
        };
        std::vector<RankedCell> continentalCells;
        double continentalArea = 0.0;
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const std::size_t index = static_cast<std::size_t>(la) * lonS + lo;
                auto& c = build.Cell(la, lo);
                if (isContinentalCrust[index])
                {
                    const double area = std::max(0.0, std::cos(c.latitudeDegrees * std::numbers::pi / 180.0));
                    continentalCells.push_back({ c.crustAge, index, area });
                    continentalArea += area;
                }
                else
                {
                    c.crustAgeMyr = std::clamp(c.crustAge, 0.0, 1.0) * kOceanicCrustAgeScaleMyr;
                }
            }
        }
        std::sort(continentalCells.begin(), continentalCells.end(),
            [](const RankedCell& first, const RankedCell& second)
            {
                return first.age != second.age ? first.age < second.age : first.index < second.index;
            });
        double cumulativeArea = 0.0;
        for (const RankedCell& ranked : continentalCells)
        {
            const double rank = continentalArea > 0.0
                ? (cumulativeArea + 0.5 * ranked.area) / continentalArea : 0.5;
            cumulativeArea += ranked.area;
            const double youngShare = 1.0 - kCratonAreaShare;
            const double ageFraction = rank < youngShare
                ? kYoungestContinentalAgeFraction +
                    (kCratonAgeFraction - kYoungestContinentalAgeFraction) * (rank / youngShare)
                : kCratonAgeFraction + (1.0 - kCratonAgeFraction) * ((rank - youngShare) / kCratonAreaShare);
            build.data.grid[ranked.index].crustAgeMyr = std::clamp(ageFraction, 0.0, 1.0) * planetAgeMyr;
        }
    }
}
