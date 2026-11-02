#include "world/WorkSystem.h"

#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    void WorkSystem::Tick(World& world, const Grid& grid, float fixedDeltaTime)
    {
        if (!std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
        {
            throw std::invalid_argument("Work system time step must be finite and greater than zero.");
        }

        for (std::size_t index = 0; index < m_orders.size();)
        {
            const bool orderIsValid = AdvanceOrder(m_orders[index], world, grid, fixedDeltaTime);
            const WorldObject* worker = world.FindObject(m_orders[index].worker);
            const WorldObject* targetObject = m_orders[index].targetObject.has_value()
                ? world.FindObject(*m_orders[index].targetObject)
                : nullptr;
            const Block* targetBlock = m_orders[index].type == WorkType::DigBlock
                ? world.GetBlock(m_orders[index].targetCell)
                : nullptr;
            if (!orderIsValid || worker == nullptr || worker->type != WorldObjectType::Worker ||
                (m_orders[index].type == WorkType::ChopTree &&
                 (targetObject == nullptr || targetObject->type != WorldObjectType::Tree)) ||
                (m_orders[index].type == WorkType::DigBlock &&
                 (targetBlock == nullptr || targetBlock->type == BlockType::Air)))
            {
                m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            if (m_orders[index].phase == WorkPhase::Working &&
                m_orders[index].workProgress >= GetWorkDuration(m_orders[index]))
            {
                if (m_orders[index].type == WorkType::ChopTree)
                {
                    world.TransformObject(
                        *m_orders[index].targetObject,
                        WorldObjectType::Item,
                        Item{ ItemType::Logs, m_config.logsPerTree });
                }
                else
                {
                    world.SetBlockType(m_orders[index].targetCell, BlockType::Air);
                }
                m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            ++index;
        }
    }

    WorkerActivity WorkSystem::GetActivity(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker)
            {
                return order.phase == WorkPhase::Moving
                    ? WorkerActivity::Moving
                    : WorkerActivity::Working;
            }
        }
        return WorkerActivity::Idle;
    }

    float WorkSystem::GetWorkProgress(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker && order.phase == WorkPhase::Working)
            {
                return order.workProgress / GetWorkDuration(order);
            }
        }
        return 0.0f;
    }

    std::vector<GridCell> WorkSystem::GetRemainingPath(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker && order.phase == WorkPhase::Moving &&
                order.nextPathCell < order.path.size())
            {
                return {
                    order.path.begin() + static_cast<std::ptrdiff_t>(order.nextPathCell),
                    order.path.end()
                };
            }
        }
        return {};
    }

    float WorkSystem::GetWorkDuration(const WorkOrder& order) const
    {
        return order.type == WorkType::ChopTree
            ? m_config.choppingDuration
            : m_config.diggingDuration;
    }

    bool WorkSystem::AdvanceOrder(WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime)
    {
        const WorldObject* worker = world.FindObject(order.worker);
        if (worker == nullptr)
        {
            return false;
        }
        if (order.type == WorkType::ChopTree)
        {
            const WorldObject* tree = world.FindObject(*order.targetObject);
            if (tree == nullptr || tree->type != WorldObjectType::Tree)
            {
                return false;
            }
        }
        else
        {
            const Block* block = world.GetBlock(order.targetCell);
            if (block == nullptr || block->type == BlockType::Air)
            {
                return false;
            }
        }

        if (order.phase == WorkPhase::Moving)
        {
            return AdvanceMovement(order, world, grid, fixedDeltaTime);
        }

        order.workProgress += fixedDeltaTime;
        return true;
    }

    bool WorkSystem::AdvanceMovement(
        WorkOrder& order,
        World& world,
        const Grid& grid,
        float fixedDeltaTime)
    {
        float remainingMovement = m_config.movementSpeed * fixedDeltaTime;
        while (remainingMovement > 0.0f && order.nextPathCell < order.path.size())
        {
            const WorldObject* worker = world.FindObject(order.worker);
            if (worker == nullptr)
            {
                return false;
            }

            const GridCell nextCell = order.path[order.nextPathCell];
            const std::optional<WorldObjectId> occupant = world.FindObjectAt(nextCell);
            if (occupant.has_value() && *occupant != order.worker)
            {
                const std::optional<std::vector<GridCell>> newPath =
                    FindPath(worker->cell, order.workCell, order.worker, world, grid);
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
                if (!world.SetObjectCell(order.worker, nextCell, grid))
                {
                    return false;
                }
                remainingMovement -= distance;
                ++order.nextPathCell;
                continue;
            }

            const float movementRatio = remainingMovement / distance;
            world.SetObjectPosition(
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
            order.phase = WorkPhase::Working;
        }
        return true;
    }
}
