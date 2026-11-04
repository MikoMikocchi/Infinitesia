#include "gameplay/work/WorkPathfinding.h"

#include "gameplay/work/WorkUtils.h"

namespace Infinitesia {
namespace WorkInternal {
std::optional<std::vector<GridCell>> FindPath(
    GridPathfinder& pathfinder, GridCell start, GridCell goal,
    WorldObjectId worker, const GameWorld& world, const Grid& grid,
    const std::vector<WorkSystem::WorkOrder>& orders) {
  return pathfinder.FindPath(start, goal, grid, [&](GridCell cell) {
    const std::optional<WorldObjectId> occupant =
        world.entities.FindObjectAt(cell);
    const bool occupiedByAnotherObject =
        occupant.has_value() && *occupant != worker;
    const Block* block = world.terrain.GetBlock(cell);
    const WorldObject* workerObject = world.entities.FindObject(worker);
    const bool isWorkersCurrentCell =
        workerObject != nullptr && workerObject->cell.x == cell.x &&
        workerObject->cell.y == cell.y && workerObject->cell.z == cell.z;
    const bool isWalkable =
        block != nullptr && (world.terrain.Registry().Walkable(block->state) ||
                             isWorkersCurrentCell);
    const bool reservedForAnotherOrder =
        WorkInternal::IsReserved(orders, cell) &&
        (cell.x != goal.x || cell.y != goal.y || cell.z != goal.z);
    return isWalkable && !occupiedByAnotherObject && !reservedForAnotherOrder;
  });
}
}  // namespace WorkInternal
}  // namespace Infinitesia
