#pragma once

#include "geology/catalog/GeologyCatalog.h"
#include "planet/PlanetMap.h"
#include "terrain_snapshot/Options.h"

#include <map>

namespace SnapshotTool
{
    // The rocks that were drawn, most visible first, with their colours.
    void PrintRocks(const Infinitesia::GeologyCatalog& catalog,
                    const std::map<Infinitesia::MaterialId, long>& seen);

    // The geological setting of the planet cell under the window centre.
    void PrintPlanetCell(const Options& options, const Infinitesia::PlanetMap& map);
}
