#pragma once

#include <optional>
#include <vector>

#include "gameplay/GameWorld.h"
#include "gameplay/navigation/GridPathfinder.h"
#include "gameplay/work/WorkSystem.h"
#include "world/Grid.h"

namespace Infinitesia {
namespace WorkInternal {
std::optional<std::vector<GridCell>> FindPath(
    GridPathfinder& pathfinder, GridCell start, GridCell goal,
    WorldObjectId worker, const GameWorld& world, const Grid& grid,
    const std::vector<WorkSystem::WorkOrder>& orders);
}
}  // namespace Infinitesia
