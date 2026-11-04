#pragma once

#include <map>

#include "domains/natural/planetary/PlanetMap.h"
#include "matter/MatterCatalog.h"
#include "terrain_snapshot/Options.h"

namespace SnapshotTool {
// The rocks that were drawn, most visible first, with their colours.
void PrintRocks(const Infinitesia::MatterCatalog& catalog,
                const std::map<Infinitesia::MatterId, long>& seen);

// The geological setting of the planet cell under the window centre.
void PrintPlanetCell(const Options& options, const Infinitesia::PlanetMap& map);
}  // namespace SnapshotTool
