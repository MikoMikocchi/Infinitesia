#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <queue>
#include <vector>

namespace Infinitesia
{
    void PlanetMap::GenerateHydrology()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();

        const int dx[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        const int dy[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };

        // -----------------------------------------------------------------------
        // Precompute per-latitude cell area weights (spherical shell segment).
        //
        // Area of a latitude-longitude cell on a sphere of radius R:
        //   A_ij = R² · Δλ · (sin φ_N - sin φ_S)
        //
        // where Δλ is the longitude width in radians, φ_N and φ_S are the
        // northern and southern latitude boundaries of the cell.
        //
        // We normalise by the mean cell area so that the water-balance numbers
        // remain comparable to the mm/yr precipitation values.  The area ratio
        // ensures that equatorial cells (which are much larger than polar cells
        // on a lat-lon grid) contribute proportionally more runoff volume.
        // -----------------------------------------------------------------------
        const double radiusKm = m_planet.GetRadius() / 1000.0;
        const double deltaLonRad = (2.0 * std::numbers::pi) / lonS;
        const double deltaLatRad = std::numbers::pi / latB;
        std::vector<double> cellAreaKm2(static_cast<std::size_t>(latB));
        double meanCellAreaKm2 = 0.0;
        for (int la = 0; la < latB; ++la)
        {
            const double phiS = -std::numbers::pi / 2.0 + la * deltaLatRad;
            const double phiN = phiS + deltaLatRad;
            cellAreaKm2[static_cast<std::size_t>(la)] =
                radiusKm * radiusKm * deltaLonRad *
                (std::sin(phiN) - std::sin(phiS));
            meanCellAreaKm2 += cellAreaKm2[static_cast<std::size_t>(la)];
        }
        meanCellAreaKm2 /= latB;

        // -----------------------------------------------------------------------
        // Step 1: Depression filling (Priority-Flood).
        //
        // D8 flow routing on a raw DEM produces spurious inland sinks caused
        // by terrain noise.  Priority-Flood fills every depression up to its
        // lowest spill elevation so that all land cells (except genuine
        // endorheic basins) can drain to the ocean.
        //
        // On a planet with no ocean (fully land-covered), we fall back to
        // seeding the Priority-Flood from the global elevation minimum so that
        // all depressions are still processed and the drainage network
        // terminates at the lowest point on the surface.
        // -----------------------------------------------------------------------
        const std::size_t cellCount = static_cast<std::size_t>(latB) * lonS;
        std::vector<double> filledElev(cellCount);
        for (std::size_t i = 0; i < cellCount; ++i)
            filledElev[i] = m_grid[i].elevationNormalized;

        std::vector<bool> processed(cellCount, false);
        using HeapEntry = std::pair<double, int>;
        std::priority_queue<
            HeapEntry,
            std::vector<HeapEntry>,
            std::greater<HeapEntry>> heap;

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                if (Cell(la, lo).isOcean)
                {
                    const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                    processed[idx] = true;
                    heap.emplace(filledElev[idx], static_cast<int>(idx));
                }
            }
        }

        // Fallback for fully land-covered planets: seed from the global minimum.
        if (heap.empty())
        {
            int minIdx = 0;
            for (std::size_t i = 1; i < cellCount; ++i)
                if (filledElev[i] < filledElev[static_cast<std::size_t>(minIdx)])
                    minIdx = static_cast<int>(i);
            processed[static_cast<std::size_t>(minIdx)] = true;
            heap.emplace(filledElev[static_cast<std::size_t>(minIdx)], minIdx);
        }

        constexpr double kEpsilon = 1e-7;
        while (!heap.empty())
        {
            const auto [elev, cellIdx] = heap.top();
            heap.pop();

            const int la = cellIdx / lonS;
            const int lo = cellIdx % lonS;

            for (int d = 0; d < 8; ++d)
            {
                const int nla = la + dx[d];
                const int nlo = ((lo + dy[d]) + lonS) % lonS;
                if (nla < 0 || nla >= latB)
                    continue;
                const std::size_t nIdx = static_cast<std::size_t>(nla) * lonS + nlo;
                if (processed[nIdx])
                    continue;
                const double newElev = std::max(filledElev[nIdx], elev + kEpsilon);
                filledElev[nIdx] = newElev;
                processed[nIdx] = true;
                heap.emplace(newElev, static_cast<int>(nIdx));
            }
        }

        // -----------------------------------------------------------------------
        // Step 2: D8 flow direction on the filled DEM.
        // -----------------------------------------------------------------------
        std::vector<int> flowDir(cellCount, -1);
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                if (Cell(la, lo).isOcean)
                    continue;

                const double cellElev = filledElev[idx];
                double minElev = cellElev;
                int bestNeighbor = -1;
                for (int d = 0; d < 8; ++d)
                {
                    const int nla = la + dx[d];
                    const int nlo = ((lo + dy[d]) + lonS) % lonS;
                    if (nla < 0 || nla >= latB)
                        continue;
                    const double ne =
                        filledElev[static_cast<std::size_t>(nla) * lonS + nlo];
                    if (ne < minElev) { minElev = ne; bestNeighbor = nla * lonS + nlo; }
                }
                flowDir[idx] = bestNeighbor;
            }
        }

        // -----------------------------------------------------------------------
        // Step 3: Topological sort (high → low) for upstream accumulation.
        // -----------------------------------------------------------------------
        std::vector<int> order(static_cast<int>(cellCount));
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b)
        {
            return filledElev[static_cast<std::size_t>(a)] >
                   filledElev[static_cast<std::size_t>(b)];
        });

        // -----------------------------------------------------------------------
        // Step 4: Area-weighted water-balance accumulation.
        //
        //   Q_out = Q_in + max(0, P − ET) × (A_cell / A_mean)
        //
        // Weighting by cell area is essential on a lat-lon grid: an equatorial
        // cell covers ~cos(0°)/cos(89°) ≈ 57× more surface than a polar cell.
        // Without the weight, rivers draining through high-latitude headwaters
        // appear artificially powerful compared to rivers in the tropics.
        //
        // ET approximation (Penman-inspired linear fit):
        //   At  0 °C → ET ≈ 120 mm/yr (cold, low evaporation)
        //   At 30 °C → ET ≈ 480 mm/yr (warm, high evaporation)
        //
        // waterAccum units: mm/yr · (dimensionless area ratio), accumulated
        // upstream.  drainageAccumulation is this value normalised by the
        // global maximum, giving a 0..1 relative discharge that is both
        // resolution-independent and planet-size-independent.
        // -----------------------------------------------------------------------
        std::vector<double> waterAccum(cellCount, 0.0);
        for (const int idx : order)
        {
            const int la = idx / lonS;
            const auto& c = Cell(la, idx % lonS);
            if (c.isOcean)
                continue;

            const double etAnnualMm =
                std::clamp(120.0 + c.temperatureCelsius * 12.0, 0.0, 600.0);
            const double runoffMm = std::max(0.0, c.precipitationMm - etAnnualMm);
            const double areaRatio =
                meanCellAreaKm2 > 0.0
                    ? cellAreaKm2[static_cast<std::size_t>(la)] / meanCellAreaKm2
                    : 1.0;
            waterAccum[static_cast<std::size_t>(idx)] += runoffMm * areaRatio;

            const int target = flowDir[static_cast<std::size_t>(idx)];
            if (target >= 0)
                waterAccum[static_cast<std::size_t>(target)] +=
                    waterAccum[static_cast<std::size_t>(idx)];
        }

        const double maxWater =
            *std::max_element(waterAccum.begin(), waterAccum.end());

        // -----------------------------------------------------------------------
        // Step 5: Classify rivers and lakes.
        //
        // Lakes: a cell is a genuine lake candidate only when the Priority-Flood
        // raised its elevation by more than a noise epsilon (fillDelta test).
        // This separates true closed-basin sinks from cells that were given only
        // a tiny upward nudge to maintain a drainage gradient across a flat area.
        // We additionally require non-trivial upstream drainage to exclude
        // isolated single-cell pits with no contributing catchment.
        // -----------------------------------------------------------------------
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = Cell(la, lo);
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;

                c.drainageAccumulation = maxWater > 0.0
                    ? waterAccum[idx] / maxWater
                    : 0.0;

                constexpr double kRiverDischargeThreshold = 0.005;
                c.hasRiver = !c.isOcean &&
                    c.drainageAccumulation > kRiverDischargeThreshold;
                c.riverDischarge = c.hasRiver ? c.drainageAccumulation : 0.0;

                constexpr double kLakeFillThreshold = 0.001;
                const double fillDelta = filledElev[idx] - c.elevationNormalized;
                c.isLake = !c.isOcean &&
                    fillDelta > kLakeFillThreshold &&
                    c.drainageAccumulation > 0.002;
            }
        }
    }
}
