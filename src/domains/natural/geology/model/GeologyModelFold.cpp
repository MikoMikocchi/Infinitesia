#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/geology/util/StableRandom.h"

namespace Infinitesia {
double GeologyModel::Displacement(const Structure& structure, double eastMeters,
                                  double northMeters) noexcept {
  double lift =
      structure.slopeEast * eastMeters + structure.slopeNorth * northMeters;
  if (!structure.folds.empty()) {
    // The fold axes bend where the warp noise says, so folds are not straight
    // cylinders.
    const double warp =
        structure.warpRadians *
        GeologyRandom::ValueNoise2(
            structure.seed, eastMeters / structure.warpWavelengthMeters,
            northMeters / structure.warpWavelengthMeters);
    for (const Fold& fold : structure.folds) {
      const double along =
          fold.directionEast * eastMeters + fold.directionNorth * northMeters;
      lift += fold.amplitudeMeters *
              std::sin(2.0 * std::numbers::pi * along / fold.wavelengthMeters +
                       fold.phase + warp);
    }
  }
  return lift;
}

double GeologyModel::Displacement(ProvinceId province, double eastMeters,
                                  double northMeters) const noexcept {
  return Displacement(StructureOf(province), eastMeters, northMeters);
}

GeologyModel::ColumnProfile GeologyModel::BaseProfileAt(
    ProvinceId province, double eastMeters, double northMeters,
    int pileTopMeters) const {
  const std::vector<Layer>& pile = PileOf(province);
  const Structure& structure = StructureOf(province);
  const double lift = Displacement(structure, eastMeters, northMeters);

  ColumnProfile profile;
  double height =
      pileTopMeters;  // stratigraphic height of the top of the current layer
  const std::size_t count =
      std::min<std::size_t>(pile.size(), kMaxProfileLayers);
  for (std::size_t i = 0; i < count; ++i) {
    const bool basement = i + 1 == count;
    double bottomHeight = std::numeric_limits<double>::lowest();
    if (!basement) {
      const LayerShape& shape = structure.layers[i];
      const double noise = GeologyRandom::ValueNoise2(
          shape.noiseSeed, eastMeters / shape.noiseWavelengthMeters,
          northMeters / shape.noiseWavelengthMeters);
      // A layer thins to nothing where the variation takes all of its
      // thickness.
      const double thickness = std::max(
          0.0, pile[i].thicknessMeters * (1.0 + shape.variability * noise));
      if (thickness <= 0.0) {
        continue;  // pinched out here
      }
      bottomHeight = height - thickness;
    }

    const int bottom = basement
                           ? std::numeric_limits<int>::min()
                           : static_cast<int>(std::lround(bottomHeight + lift));
    if (profile.count > 0 &&
        profile.rock[static_cast<std::size_t>(profile.count - 1)] ==
            pile[i].rock) {
      // The layer between two pieces of one rock has pinched out: they are one
      // layer.
      profile.bottom[static_cast<std::size_t>(profile.count - 1)] = bottom;
    } else {
      const int slot = profile.count++;
      profile.rock[static_cast<std::size_t>(slot)] = pile[i].rock;
      profile.bottom[static_cast<std::size_t>(slot)] = bottom;
      profile.formation[static_cast<std::size_t>(slot)] =
          static_cast<std::uint8_t>(i);
    }
    height = bottomHeight;
  }
  if (profile.count == 0) {
    profile.count = 1;
    profile.rock[0] = pile.back().rock;
    profile.bottom[0] = std::numeric_limits<int>::min();
  }
  return profile;
}
}  // namespace Infinitesia
