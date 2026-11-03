#include "planet/stages/tectonics/TectonicsInternal.h"

#include "planet/internal/GenerationMath.h"

#include <algorithm>
#include <utility>

namespace Infinitesia::TectonicsDetail
{
    using namespace GenerationDetail;

    void SmoothTectonicRelief(PlanetBuild& build)
    {
        const int latB = build.LatBands();
        const int lonS = build.LonSegs();

        // Boundary subtype can change abruptly between adjacent cells even when
        // both lie on the same fault. Blur the signed relief field so those
        // classification changes become broad mountain belts and trenches.
        std::vector<double> relief(
            static_cast<std::size_t>(latB) * static_cast<std::size_t>(lonS));
        for (int la = 0; la < latB; ++la)
            for (int lo = 0; lo < lonS; ++lo)
                relief[static_cast<std::size_t>(la) * lonS + lo] =
                    build.Cell(la, lo).tectonicReliefKm;

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
                build.Cell(la, lo).tectonicReliefKm =
                    relief[static_cast<std::size_t>(la) * lonS + lo];
    }
}
