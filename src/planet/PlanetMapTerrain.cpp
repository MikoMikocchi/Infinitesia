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
                const double fineDetail = SphericalFbm(
                    c.latitudeDegrees, c.longitudeDegrees, seed ^ 0x74657272u, 18, 4);
                elev += (detail - 0.5) * 0.9 + (fineDetail - 0.5) * 0.3;

                double nearbyMountainRelief = 0.0;
                int nearbyWeight = 0;
                for (int latitudeOffset = -2; latitudeOffset <= 2; ++latitudeOffset)
                {
                    for (int longitudeOffset = -2; longitudeOffset <= 2; ++longitudeOffset)
                    {
                        const int neighborLatitude =
                            std::clamp(la + latitudeOffset, 0, latB - 1);
                        const int neighborLongitude =
                            (lo + longitudeOffset + lonS) % lonS;
                        nearbyMountainRelief += std::max(
                            0.0,
                            Cell(neighborLatitude, neighborLongitude).tectonicReliefKm);
                        ++nearbyWeight;
                    }
                }
                nearbyMountainRelief /= nearbyWeight;
                const double localMountainRelief =
                    std::max(0.0, c.tectonicReliefKm);
                const double forelandBasin =
                    std::clamp(nearbyMountainRelief - localMountainRelief, 0.0, 1.8);
                if (c.continentalCrustFraction > 0.5)
                {
                    elev -= forelandBasin * 0.45;
                    if (c.surfaceRock == RockType::GraniticShield)
                    {
                        elev += std::clamp(
                            (c.crustAge - 0.65) / 0.35, 0.0, 1.0) * 0.45;
                    }
                    else if (c.surfaceRock == RockType::SedimentaryBasin)
                    {
                        elev -= std::clamp(
                            (0.65 - c.crustAge) / 0.65, 0.0, 1.0) * 0.3;
                    }
                }
                const double mountainFraction =
                    std::clamp(localMountainRelief / 4.5, 0.0, 1.0);
                const double ridgeNoise =
                    1.0 - std::abs(2.0 * fineDetail - 1.0);
                elev += (ridgeNoise - 0.5) * mountainFraction * 0.9;
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

                // Central-difference elevation gradients in km.
                // The latitude step is fixed at (180/latB) degrees per cell.
                // The longitude step shrinks with cos(lat) because meridians
                // converge toward the poles; without this correction the
                // east-west gradient is overestimated at high latitudes.
                const double latStepKm =
                    (180.0 / latB) * (std::numbers::pi / 180.0) * 6371.0;
                const double cosLat =
                    std::max(1e-3, std::cos(c.latitudeDegrees *
                                            std::numbers::pi / 180.0));
                const double lonStepKm =
                    (360.0 / lonS) * (std::numbers::pi / 180.0) * 6371.0 *
                    cosLat;

                // Two-cell central-difference horizontal distances (km):
                const double dxLat = 2.0 * latStepKm;
                const double dxLon = 2.0 * lonStepKm;

                const double dElevLat =
                    (Cell(la2, lo).elevationNormalized -
                     Cell(la1, lo).elevationNormalized) * kElevationRangeKm; // km
                const double dElevLon =
                    (Cell(la, lo2).elevationNormalized -
                     Cell(la, lo1).elevationNormalized) * kElevationRangeKm; // km

                // Gradient magnitude [km/km = dimensionless].
                // Normalise by a reference slope of 0.05 (rise/run ≈ 3°) so
                // that typical mountain flanks read ~0.5 and not ~0.001.
                constexpr double kRefSlope = 0.05;
                const double rise = std::hypot(dElevLat / dxLat, dElevLon / dxLon);
                c.slopeNormalized = std::clamp(rise / kRefSlope, 0.0, 1.0);
            }
        }
    }
}
