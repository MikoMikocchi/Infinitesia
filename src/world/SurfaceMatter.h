#pragma once

#include "blocks/BlockRegistry.h"

namespace Infinitesia {
// The materials a terrain source hands out for the ground it makes, found once
// by key in the registry (docs/06_BLOCKS.md, section 4).
struct SurfaceMatter {
  MatterId turf = kAirMatter;     // soil.turf: the surface of grassland
  MatterId subsoil = kAirMatter;  // soil.subsoil: dirt
  MatterId sand = kAirMatter;     // soil.sand
  MatterId bedrock =
      kAirMatter;               // rock.bedrock: stone when there is no geology
  MatterId water = kAirMatter;  // fluid.water: the default surface fluid

  // Throws CatalogError naming the first key the registry lacks.
  static SurfaceMatter Resolve(const BlockRegistry& registry);
};
}  // namespace Infinitesia
