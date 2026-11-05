#pragma once

#include <optional>
#include <vector>

#include "gameplay/GameWorld.h"
#include "gameplay/navigation/GridPathfinder.h"
#include "gameplay/work/WorkSystem.h"
#include "world/Grid.h"

namespace Infinitesia
{
namespace WorkInternal
{
bool HasOrderForWorker(const std::vector<WorkSystem::WorkOrder> &orders, WorldObjectId worker);
bool HasOrderForTree(const std::vector<WorkSystem::WorkOrder> &orders, const std::vector<GridCell> &wood);
bool HasOrderForDigCell(const std::vector<WorkSystem::WorkOrder> &orders, GridCell cell);
bool IsReserved(const std::vector<WorkSystem::WorkOrder> &orders, GridCell cell);
float GetWorkDuration(const WorkerConfig &config, const WorkSystem::WorkOrder &order);
} // namespace WorkInternal
} // namespace Infinitesia
