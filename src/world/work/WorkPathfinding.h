#pragma once

#include "world/Grid.h"
#include "world/GridPathfinder.h"
#include "world/World.h"
#include "world/WorkSystem.h"

#include <optional>
#include <vector>

namespace Infinitesia
{
    namespace WorkInternal
    {
        std::optional<std::vector<GridCell>> FindPath(
            GridPathfinder& pathfinder,
            GridCell start,
            GridCell goal,
            WorldObjectId worker,
            const World& world,
            const Grid& grid,
            const std::vector<WorkSystem::WorkOrder>& orders);
    }
}
