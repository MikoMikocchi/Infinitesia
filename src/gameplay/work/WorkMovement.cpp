#include "gameplay/work/WorkMovement.h"
#include "gameplay/work/WorkPathfinding.h"
#include "gameplay/work/WorkUtils.h"

#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    namespace WorkInternal
    {
        bool AdvanceMovement(WorkSystem& system, WorkSystem::WorkOrder& order, GameWorld& world, const Grid& grid, float fixedDeltaTime)
        {
            float remainingMovement = system.GetMovementSpeed() * fixedDeltaTime;
            while (remainingMovement > 0.0f && order.nextPathCell < order.path.size())
            {
                const WorldObject* worker = world.entities.FindObject(order.worker);
                if (worker == nullptr)
                {
                    return false;
                }

                const GridCell nextCell = order.path[order.nextPathCell];
                const std::optional<WorldObjectId> occupant = world.entities.FindObjectAt(nextCell);
                if (occupant.has_value() && *occupant != order.worker)
                {
                    const std::optional<std::vector<GridCell>> newPath =
                        FindPath(system.GetPathfinder(), worker->cell, order.workCell, order.worker, world, grid, system.GetOrders());
                    if (!newPath.has_value() || newPath->size() <= 1)
                    {
                        return false;
                    }
                    order.path = *newPath;
                    order.nextPathCell = 1;
                    continue;
                }

                const WorldPosition destination = grid.CellToSurface(nextCell);
                const float deltaX = destination.x - worker->position.x;
                const float deltaZ = destination.z - worker->position.z;
                const float distance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);

                if (distance <= remainingMovement)
                {
                    if (!world.entities.SetObjectCell(order.worker, nextCell, grid))
                    {
                        return false;
                    }
                    remainingMovement -= distance;
                    ++order.nextPathCell;
                    continue;
                }

                const float movementRatio = remainingMovement / distance;
                world.entities.SetObjectPosition(
                    order.worker,
                    {
                        worker->position.x + deltaX * movementRatio,
                        worker->position.y,
                        worker->position.z + deltaZ * movementRatio
                    });
                remainingMovement = 0.0f;
            }

            if (order.nextPathCell >= order.path.size())
            {
                order.phase = WorkSystem::WorkPhase::Working;
            }
            return true;
        }

        bool AdvanceOrder(WorkSystem& system, WorkSystem::WorkOrder& order, GameWorld& world, const Grid& grid, float fixedDeltaTime)
        {
            const WorldObject* worker = world.entities.FindObject(order.worker);
            if (worker == nullptr)
            {
                return false;
            }
            if (order.type == WorkSystem::WorkType::ChopTree)
            {
                const WorldObject* tree = world.entities.FindObject(*order.targetObject);
                if (tree == nullptr || !tree->Is(ObjectAbility::Choppable))
                {
                    return false;
                }
            }
            else
            {
                const Block* block = world.terrain.GetBlock(order.targetCell);
                if (block == nullptr || !world.terrain.Registry().Occupied(block->state))
                {
                    return false;
                }
            }

            if (order.phase == WorkSystem::WorkPhase::Moving)
            {
                return AdvanceMovement(system, order, world, grid, fixedDeltaTime);
            }

            order.workProgress += fixedDeltaTime;
            return true;
        }
    }
}
