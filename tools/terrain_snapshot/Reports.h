#pragma once

#include "materials/MaterialCatalog.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "terrain_snapshot/Options.h"

#include <map>

namespace SnapshotTool
{
    // The rocks that were drawn, most visible first, with their colours.
    void PrintRocks(const Infinitesia::MaterialCatalog& catalog,
                    const std::map<Infinitesia::MaterialId, long>& seen);

    // The geological setting of the planet cell under the window centre.
    void PrintPlanetCell(const Options& options, const Infinitesia::PlanetMap& map);
}
