#pragma once

#include <vector>

#include "domains/natural/planetary/Planet.h"
#include "domains/natural/planetary/PlanetRegion.h"
#include "domains/natural/planetary/geology/PlanetEpochs.h"
#include "domains/natural/planetary/geology/Province.h"
#include "domains/natural/planetary/pipeline/PlanetLayers.h"

namespace Infinitesia {
// Everything a planet is made of, as the stages fill it in and PlanetMap serves
// it.
struct PlanetData {
  Planet planet;
  ClimateInputs climate;
  std::vector<PlanetRegion> grid;  // row-major [lat * lonSegments + lon]
  std::vector<Province> provinces;
  PlanetEpochTable epochs;
  PlanetWaterBalance waterBalance;
  PlanetLayers layers;
};
}  // namespace Infinitesia
