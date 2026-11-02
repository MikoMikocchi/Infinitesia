#include "planet/PlanetMap.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace Infinitesia
{
    void PlanetMap::GenerateHydrology()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();

        std::vector<int> accumulation(static_cast<std::size_t>(latB) * lonS, 0);
        std::vector<int> flowDir(static_cast<std::size_t>(latB) * lonS, -1);

        const int dx[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
        const int dy[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };

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
                    if (ne < minElev) { minElev = ne; bestNeighbor = nla * lonS + nlo; }
                }
                flowDir[static_cast<std::size_t>(la) * lonS + lo] = bestNeighbor;
            }
        }

        std::vector<int> order(static_cast<std::size_t>(latB) * lonS);
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b)
        {
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
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                const int acc = accumulation[idx];
                c.drainageAccumulation = maxAcc > 0
                    ? static_cast<double>(acc) / maxAcc : 0.0;

                constexpr double kDrainageThreshold = 0.03;
                constexpr double kMinPrecipForRiver = 50.0;
                const double waterBudget =
                    std::clamp(c.precipitationMm / 400.0, 0.0, 1.0);

                c.hasRiver = !c.isOcean
                    && c.precipitationMm >= kMinPrecipForRiver
                    && c.drainageAccumulation * waterBudget > kDrainageThreshold;
                c.riverDischarge = c.hasRiver ? c.drainageAccumulation * waterBudget : 0.0;
                c.isLake = !c.isOcean
                    && flowDir[idx] == -1
                    && c.drainageAccumulation > 0.005;
            }
        }
    }
}
