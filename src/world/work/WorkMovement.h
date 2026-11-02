#pragma once

#include "world/World.h"
#include "world/WorkSystem.h"

namespace Infinitesia
{
    namespace WorkInternal
    {
        bool AdvanceOrder(WorkSystem& system, WorkSystem::WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime);
        bool AdvanceMovement(WorkSystem& system, WorkSystem::WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime);
    }
}
