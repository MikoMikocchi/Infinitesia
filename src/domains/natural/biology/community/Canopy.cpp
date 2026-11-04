#include "domains/natural/biology/community/Canopy.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia {
double CanopyCover(const Conditions& conditions) noexcept {
  constexpr double kClosedCanopyGrams = 1500.0;
  const double byTemperature =
      3000.0 / (1.0 + std::exp(1.315 - 0.119 * conditions.temperatureCelsius));
  const double byPrecipitation =
      3000.0 *
      (1.0 - std::exp(-0.000664 * std::max(0.0, conditions.precipitationMm)));
  double cover = std::min(byTemperature, byPrecipitation) / kClosedCanopyGrams;

  // Up to two dry months cost nothing; every month beyond takes a tenth, and
  // trees thin out into savanna.
  cover *= 1.0 - 0.1 * std::clamp(conditions.drySeasonMonths - 2.0, 0.0, 6.0);
  // Poor or shallow soil carries fewer trees.
  cover *= 0.6 + 0.4 * std::clamp(conditions.soilFertility, 0.0, 1.0);
  cover *= 0.6 + 0.4 * std::clamp(conditions.soilDepth, 0.0, 1.0);
  return std::clamp(cover, 0.0, 1.0);
}
}  // namespace Infinitesia
