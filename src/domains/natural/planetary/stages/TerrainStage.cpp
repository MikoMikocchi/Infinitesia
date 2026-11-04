#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/internal/GenerationMath.h"
#include "domains/natural/planetary/internal/PlateKinematics.h"
#include "domains/natural/planetary/stages/PlanetStages.h"

namespace Infinitesia {
using namespace GenerationDetail;

void RunTerrain(PlanetBuild& build) {
  const int latB = build.LatBands();
  const int lonS = build.LonSegs();
  const std::uint32_t seed = build.data.planet.GetSeed();

  const double ageFrac =
      std::clamp(build.data.climate.planetAgeYears / 4.5e9, 0.0, 1.0);
  const double gravScale =
      build.data.climate.gravityMetersPerSecondSquared / 9.81;
  std::vector<double> elevKm(static_cast<std::size_t>(latB) * lonS);

  for (int la = 0; la < latB; ++la) {
    for (int lo = 0; lo < lonS; ++lo) {
      const auto& c = build.Cell(la, lo);
      const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
      double elev = 0.0;

      const double oceanicElevation = OceanicBathymetryKm(c.crustAge, 180.0);
      const double continentalElevation =
          AiryIsostasyKm(c.isostaticCrustThicknessKm, kContinentalRho);
      elev = oceanicElevation +
             (continentalElevation - oceanicElevation) *
                 c.continentalCrustFraction +
             c.tectonicReliefKm;

      const double detail =
          SphericalFbm(c.latitudeDegrees, c.longitudeDegrees, seed, 4, 6);
      const double fineDetail = SphericalFbm(
          c.latitudeDegrees, c.longitudeDegrees, seed ^ 0x74657272u, 18, 4);
      elev += (detail - 0.5) * 0.9 + (fineDetail - 0.5) * 0.3;

      double nearbyMountainRelief = 0.0;
      int nearbyWeight = 0;
      for (int latitudeOffset = -2; latitudeOffset <= 2; ++latitudeOffset) {
        for (int longitudeOffset = -2; longitudeOffset <= 2;
             ++longitudeOffset) {
          const int neighborLatitude =
              std::clamp(la + latitudeOffset, 0, latB - 1);
          const int neighborLongitude = (lo + longitudeOffset + lonS) % lonS;
          nearbyMountainRelief += std::max(
              0.0,
              build.Cell(neighborLatitude, neighborLongitude).tectonicReliefKm);
          ++nearbyWeight;
        }
      }
      nearbyMountainRelief /= nearbyWeight;
      const double localMountainRelief = std::max(0.0, c.tectonicReliefKm);
      const double forelandBasin =
          std::clamp(nearbyMountainRelief - localMountainRelief, 0.0, 1.8);
      if (c.continentalCrustFraction > 0.5) {
        elev -= forelandBasin * 0.45;
        if (c.surfaceRock == RockType::GraniticShield) {
          elev += std::clamp((c.crustAge - 0.65) / 0.35, 0.0, 1.0) * 0.45;
        } else if (c.surfaceRock == RockType::SedimentaryBasin) {
          elev -= std::clamp((0.65 - c.crustAge) / 0.65, 0.0, 1.0) * 0.3;
        }
      }
      const double mountainFraction =
          std::clamp(localMountainRelief / 4.5, 0.0, 1.0);
      const double ridgeNoise = 1.0 - std::abs(2.0 * fineDetail - 1.0);
      elev += (ridgeNoise - 0.5) * mountainFraction * 0.9;
      elevKm[idx] = elev;
    }
  }

  const double smoothingStrength = 0.24 + ageFrac * 0.16;
  for (int pass = 0; pass < 3; ++pass) {
    std::vector<double> smoothed(elevKm.size());
    for (int la = 0; la < latB; ++la) {
      for (int lo = 0; lo < lonS; ++lo) {
        const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
        double neighborhoodSum = elevKm[idx] * 4.0;
        double neighborhoodWeight = 4.0;
        for (int dla = -1; dla <= 1; ++dla) {
          for (int dlo = -1; dlo <= 1; ++dlo) {
            if (dla == 0 && dlo == 0) continue;
            const int nla = std::clamp(la + dla, 0, latB - 1);
            const int nlo = ((lo + dlo) + lonS) % lonS;
            neighborhoodSum +=
                elevKm[static_cast<std::size_t>(nla) * lonS + nlo];
            neighborhoodWeight += 1.0;
          }
        }
        const double neighborhoodMean = neighborhoodSum / neighborhoodWeight;
        smoothed[idx] = Lerp(elevKm[idx], neighborhoodMean, smoothingStrength);
      }
    }
    elevKm = std::move(smoothed);
  }

  const double erosionStrength = ageFrac * 0.45 * gravScale;
  if (erosionStrength > 0.02) {
    std::vector<double> smoothed = elevKm;
    for (int la = 0; la < latB; ++la) {
      for (int lo = 0; lo < lonS; ++lo) {
        const std::size_t idx = static_cast<std::size_t>(la) * lonS + lo;
        const double elevation = elevKm[idx];
        if (elevation <= 0.0) continue;

        double sum = 0.0;
        int count = 0;
        for (int dla = -1; dla <= 1; ++dla) {
          for (int dlo = -1; dlo <= 1; ++dlo) {
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

  for (int la = 0; la < latB; ++la) {
    for (int lo = 0; lo < lonS; ++lo) {
      auto& c = build.Cell(la, lo);
      const double elevationKm =
          elevKm[static_cast<std::size_t>(la) * lonS + lo];
      c.elevationNormalized =
          std::clamp(elevationKm / kMaxElevationKm, -1.0, 1.0);
      c.isOcean = elevationKm < 0.0;
    }
  }

  for (int la = 0; la < latB; ++la) {
    for (int lo = 0; lo < lonS; ++lo) {
      auto& c = build.Cell(la, lo);
      const int la1 = std::clamp(la - 1, 0, latB - 1);
      const int la2 = std::clamp(la + 1, 0, latB - 1);
      const int lo1 = ((lo - 1) + lonS) % lonS;
      const int lo2 = ((lo + 1) + lonS) % lonS;
      // Central-difference elevation gradients in km.
      // Use the actual planet radius so that the slope calculation
      // is correct for any planet size, not just Earth.
      const double radiusKm = build.data.planet.GetRadius() / 1000.0;
      const double latStepKm =
          (180.0 / latB) * (std::numbers::pi / 180.0) * radiusKm;
      const double cosLat = std::max(
          1e-3, std::cos(c.latitudeDegrees * std::numbers::pi / 180.0));
      const double lonStepKm =
          (360.0 / lonS) * (std::numbers::pi / 180.0) * radiusKm * cosLat;

      // Actual horizontal span of the central-difference stencil.
      //
      // At the polar boundaries la1==la or la2==la (clamped), so
      // the stencil degenerates from a two-cell span to a one-cell
      // span.  Using the constant 2*step at the poles would halve
      // the denominator and underestimate the gradient.  We measure
      // the actual number of grid steps between la1 and la2 instead.
      const double latSpanKm = static_cast<double>(la2 - la1) * latStepKm;
      // Longitude always wraps, so the span is always 2 steps.
      const double lonSpanKm = 2.0 * lonStepKm;

      const double dElevLat = (build.Cell(la2, lo).elevationNormalized -
                               build.Cell(la1, lo).elevationNormalized) *
                              kMaxElevationKm;  // km
      const double dElevLon = (build.Cell(la, lo2).elevationNormalized -
                               build.Cell(la, lo1).elevationNormalized) *
                              kMaxElevationKm;  // km

      // Gradient magnitude [km/km = dimensionless].
      // Normalise by a reference slope of 0.05 (rise/run ≈ 3°) so
      // that typical mountain flanks read ~0.5.
      constexpr double kRefSlope = 0.05;
      const double gradLat = latSpanKm > 0.0 ? dElevLat / latSpanKm : 0.0;
      const double gradLon = lonSpanKm > 0.0 ? dElevLon / lonSpanKm : 0.0;
      c.slopeNormalized =
          std::clamp(std::hypot(gradLat, gradLon) / kRefSlope, 0.0, 1.0);
    }
  }
}
}  // namespace Infinitesia
