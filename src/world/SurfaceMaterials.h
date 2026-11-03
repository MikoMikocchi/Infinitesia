#pragma once

#include "blocks/BlockRegistry.h"

namespace Infinitesia
{
    // The materials a terrain source hands out for the ground it makes, found once by key in the
    // registry (docs/06_BLOCKS.md, section 4).
    struct SurfaceMaterials
    {
        MaterialId turf = kAirMaterial;     // soil.turf: the surface of grassland
        MaterialId subsoil = kAirMaterial;  // soil.subsoil: dirt
        MaterialId sand = kAirMaterial;     // soil.sand
        MaterialId bedrock = kAirMaterial;  // rock.bedrock: stone when there is no geology
        MaterialId seawater = kAirMaterial; // fluid.seawater

        // Throws CatalogError naming the first key the registry lacks.
        static SurfaceMaterials Resolve(const BlockRegistry& registry);
    };
}
