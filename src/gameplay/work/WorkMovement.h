#pragma once

#include "gameplay/GameWorld.h"
#include "gameplay/work/WorkSystem.h"

namespace Infinitesia {
namespace WorkInternal {
bool AdvanceOrder(WorkSystem& system, WorkSystem::WorkOrder& order,
                  GameWorld& world, const Grid& grid, float fixedDeltaTime);
bool AdvanceMovement(WorkSystem& system, WorkSystem::WorkOrder& order,
                     GameWorld& world, const Grid& grid, float fixedDeltaTime);
}  // namespace WorkInternal
}  // namespace Infinitesia
