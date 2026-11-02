#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>

namespace Infinitesia
{
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
                if (c.isOcean)
                {
                    c.soilFertility = 0.0;
                    c.soilDepthNorm = 0.0;
                    continue;
                }

                c.soilFertility = GenerationDetail::SoilFertility(
                    c.surfaceRock, c.temperatureCelsius, c.precipitationMm, ageFrac);
                c.soilDepthNorm = GenerationDetail::SoilDepth(
                    c.surfaceRock, c.precipitationMm, c.slopeNormalized, ageFrac);

                if (c.hasRiver && c.slopeNormalized < 0.2)
                {
                    c.soilFertility = std::min(1.0, c.soilFertility * 1.5);
                    c.soilDepthNorm = std::min(1.0, c.soilDepthNorm * 1.8);
                }

                if (c.isVolcanicZone)
                {
                    const double weatheredFrac = std::clamp(ageFrac * 2.0, 0.0, 1.0);
                    c.soilFertility = GenerationDetail::Lerp(0.1, 0.85, weatheredFrac);
                    c.soilDepthNorm = GenerationDetail::Lerp(0.1, 0.6, weatheredFrac);
                }

                if (c.temperatureCelsius < -5.0)
                {
                    const double coldFrac =
                        std::clamp((-c.temperatureCelsius - 5.0) / 25.0, 0.0, 1.0);
                    c.soilFertility *= (1.0 - coldFrac * 0.8);
                    c.soilDepthNorm *= (1.0 - coldFrac * 0.7);
                }
            }
        }
    }

    void PlanetMap::ClassifyBiomes()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
                Cell(la, lo).biome = GenerationDetail::ClassifyBiome(Cell(la, lo));
        }
    }
}
