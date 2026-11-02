#include "planet/PlanetMap.h"

#include <algorithm>
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
        // Step 1: Depression filling (simplified Priority-Flood).
        //
        // D8 flow routing produces spurious sinks in flat or noisy terrain.
        // We fill all inland depressions up to their spill elevation so that
        // every non-ocean cell (except genuine endorheic basins) can drain to
        // the ocean.  Only cells that cannot reach the ocean even after filling
        // remain as true closed basins (candidates for lakes).
        //
        // Algorithm: push all ocean-adjacent border cells onto a min-heap,
        // then relax all land cells inward.  A land cell's "filled" elevation
        // is the maximum of its original elevation and the lowest filled
        // elevation of any of its already-processed neighbours plus epsilon.
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

        // Seed: all ocean cells and land cells adjacent to the ocean
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

        constexpr double kEpsilon = 1e-7; // ensures strict downhill path exists
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
                // Fill neighbour up to at least (elev + ε) so water can
                // always flow from it back to the current cell.
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
                    const double ne = filledElev[static_cast<std::size_t>(nla) * lonS + nlo];
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
            return filledElev[a] > filledElev[b]; // process high cells first
        });

        // -----------------------------------------------------------------------
        // Step 4: Water-balance accumulation.
        //
        // Instead of counting upstream cells (dimensionless), we accumulate the
        // effective runoff volume (in mm × cell-area units) that drains through
        // each cell.  For each cell:
        //
        //   Q_out = Q_in + max(0, P - ET)
        //
        // where  P  = local precipitation (mm)
        //        ET = potential evapotranspiration (approximated from temperature)
        //
        // This allows a river to maintain significant discharge in a local
        // dry stretch if it has collected enough water from wet headwaters.
        // -----------------------------------------------------------------------
        std::vector<double> waterAccum(cellCount, 0.0);
        for (const int idx : order)
        {
            const int la = idx / lonS;
            const int lo = idx % lonS;
            const auto& c = Cell(la, lo);
            if (c.isOcean)
                continue;

            // Penman-inspired ET approximation: warmer = more evaporation.
            // At 0°C, ET ≈ 10 mm/month (120/yr); at 30°C, ≈ 40 mm/month (480/yr).
            const double etAnnualMm =
                std::clamp(120.0 + c.temperatureCelsius * 12.0, 0.0, 600.0);
            const double runoffMm = std::max(0.0, c.precipitationMm - etAnnualMm);

            waterAccum[idx] += runoffMm;
            const int target = flowDir[idx];
            if (target >= 0)
                waterAccum[target] += waterAccum[idx];
        }

        const double maxWater =
            *std::max_element(waterAccum.begin(), waterAccum.end());

        // -----------------------------------------------------------------------
        // Step 5: Classify rivers and lakes.
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

                // River: enough accumulated water flowing through this cell.
                // The discharge threshold (0.5% of global maximum) filters
                // tiny first-order streams in very dry regions.
                constexpr double kRiverDischargeThreshold = 0.005;
                c.hasRiver = !c.isOcean &&
                    c.drainageAccumulation > kRiverDischargeThreshold;
                c.riverDischarge = c.hasRiver ? c.drainageAccumulation : 0.0;

                // Lake: a genuine closed basin (no filled outlet found AND
                // the cell sits in a depression in the ORIGINAL, unfilled DEM).
                // Comparing filled vs original elevation identifies true sinks;
                // this avoids classifying flat plateaus or mildly filled cells
                // as lakes just because Dijkstra assigned them a small positive
                // epsilon above a neighbour.
                const double originalElev = c.elevationNormalized;
                const double fillDelta = filledElev[idx] - originalElev;
                // fillDelta > kLakeFillThreshold means the cell was genuinely
                // depressed (not just given an epsilon nudge).
                constexpr double kLakeFillThreshold = 0.001;
                const bool genuineSink = fillDelta > kLakeFillThreshold;
                c.isLake = !c.isOcean &&
                    genuineSink &&
                    c.drainageAccumulation > 0.002;
            }
        }
    }
}

