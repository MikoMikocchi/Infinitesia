#include "planet/PlanetMap.h"
#include "planet/internal/PlanetMapGenerationHelpers.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Infinitesia
{
    using namespace GenerationDetail;

    void PlanetMap::GenerateTerrain()
    {
        const int latB = LatBands();
        const int lonS = LonSegs();
        const std::uint32_t seed = m_planet.GetSeed();

        const double ageFrac = std::clamp(m_climate.planetAgeYears / 4.5e9, 0.0, 1.0);
        const double gravScale = m_climate.gravityMetersPerSecondSquared / 9.81;
        std::vector<double> elevKm(static_cast<std::size_t>(latB) * lonS);

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                const auto& c = Cell(la, lo);
                const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                double elev = 0.0;

                const double oceanicElevation = OceanicBathymetryKm(c.crustAge, 180.0);
                const double continentalElevation =
                    AiryIsostasyKm(
                        c.isostaticCrustThicknessKm, kContinentalRho);
                elev = oceanicElevation +
                    (continentalElevation - oceanicElevation) *
                        c.continentalCrustFraction +
                    c.tectonicReliefKm;

                const double detail = SphericalFbm(
                    c.latitudeDegrees, c.longitudeDegrees, seed, 4, 6);
                elev += (detail - 0.5) * 0.9;
                elevKm[idx] = elev;
            }
        }

        const double smoothingStrength = 0.24 + ageFrac * 0.16;
        for (int pass = 0; pass < 3; ++pass)
        {
            std::vector<double> smoothed(elevKm.size());
            for (int la = 0; la < latB; ++la)
            {
                for (int lo = 0; lo < lonS; ++lo)
                {
                    const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                    double neighborhoodSum = elevKm[idx] * 4.0;
                    double neighborhoodWeight = 4.0;
                    for (int dla = -1; dla <= 1; ++dla)
                    {
                        for (int dlo = -1; dlo <= 1; ++dlo)
                        {
                            if (dla == 0 && dlo == 0)
                                continue;
                            const int nla = std::clamp(la + dla, 0, latB - 1);
                            const int nlo = ((lo + dlo) + lonS) % lonS;
                            neighborhoodSum +=
                                elevKm[static_cast<std::size_t>(nla) * lonS + nlo];
                            neighborhoodWeight += 1.0;
                        }
                    }
                    const double neighborhoodMean =
                        neighborhoodSum / neighborhoodWeight;
                    smoothed[idx] =
                        Lerp(elevKm[idx], neighborhoodMean, smoothingStrength);
                }
            }
            elevKm = std::move(smoothed);
        }

        const double erosionStrength = ageFrac * 0.45 * gravScale;
        if (erosionStrength > 0.02)
        {
            std::vector<double> smoothed = elevKm;
            for (int la = 0; la < latB; ++la)
            {
                for (int lo = 0; lo < lonS; ++lo)
                {
                    const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
                    const double elevation = elevKm[idx];
                    if (elevation <= 0.0)
                        continue;

                    double sum = 0.0;
                    int count = 0;
                    for (int dla = -1; dla <= 1; ++dla)
                    {
                        for (int dlo = -1; dlo <= 1; ++dlo)
                        {
                            const int nla = std::clamp(la + dla, 0, latB - 1);
                            const int nlo = ((lo + dlo) + lonS) % lonS;
                            sum += elevKm[static_cast<std::size_t>(nla) * lonS + nlo];
                            ++count;
                        }
                    }
                    const double average = sum / count;
                    const double factor =
                        erosionStrength * std::clamp(elevation / 5.0, 0.0, 1.0);
                    smoothed[idx] = Lerp(elevation, average, factor);
                }
            }
            elevKm = std::move(smoothed);
        }

        constexpr double kMaxElevationKm = 9.0;
        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = Cell(la, lo);
                const double elevationKm =
                    elevKm[static_cast<std::size_t>(la) * lonS + lo];
                c.elevationNormalized =
                    std::clamp(elevationKm / kMaxElevationKm, -1.0, 1.0);
                c.isOcean = elevationKm < 0.0;
            }
        }

        for (int la = 0; la < latB; ++la)
        {
            for (int lo = 0; lo < lonS; ++lo)
            {
                auto& c = Cell(la, lo);
                const int la1 = std::clamp(la - 1, 0, latB - 1);
                const int la2 = std::clamp(la + 1, 0, latB - 1);
                const int lo1 = ((lo - 1) + lonS) % lonS;
                const int lo2 = ((lo + 1) + lonS) % lonS;
                constexpr double kElevationRangeKm = kMaxElevationKm * 2.0;
                const double dLat =
                    (Cell(la2, lo).elevationNormalized -
                     Cell(la1, lo).elevationNormalized) * kElevationRangeKm;
                const double dLon =
                    (Cell(la, lo2).elevationNormalized -
                     Cell(la, lo1).elevationNormalized) * kElevationRangeKm;
                c.slopeNormalized =
                    std::clamp(std::hypot(dLat, dLon) / (2.0 * 200.0), 0.0, 1.0);
            }
        }
    }
}
