#include "world/WorkSystem.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        constexpr std::array<GridCell, 4> kAdjacentOffsets{
            GridCell{ 0, 0, -1 },
            GridCell{ 1, 0, 0 },
            GridCell{ 0, 0, 1 },
            GridCell{ -1, 0, 0 }
        };
    }

    WorkSystem::WorkSystem(WorkerConfig config)
        : m_config(config)
    {
        if (!std::isfinite(config.movementSpeed) || config.movementSpeed <= 0.0f ||
            !std::isfinite(config.choppingDuration) || config.choppingDuration <= 0.0f ||
            !std::isfinite(config.diggingDuration) || config.diggingDuration <= 0.0f ||
            config.logsPerTree <= 0)
        {
            throw std::invalid_argument("Worker configuration contains invalid values.");
        }
    }

    WorkOrderResult WorkSystem::AssignChopOrder(
        WorldObjectId workerId,
        WorldObjectId treeId,
        World& world,
        const Grid& grid)
    {
        const WorldObject* worker = world.FindObject(workerId);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return WorkOrderResult::InvalidWorker;
        }

        const WorldObject* tree = world.FindObject(treeId);
        if (tree == nullptr || tree->type != WorldObjectType::Tree)
        {
            return WorkOrderResult::InvalidTarget;
        }

        if (HasOrderForWorker(workerId))
        {
            return WorkOrderResult::WorkerBusy;
        }
        if (HasOrderForTree(treeId))
        {
            return WorkOrderResult::TargetBusy;
        }

        bool foundWorkCell = false;
        GridCell workCell{};
        std::vector<GridCell> bestPath;
        for (const GridCell offset : kAdjacentOffsets)
        {
            const GridCell candidate{
                tree->cell.x + offset.x,
                tree->cell.y + offset.y,
                tree->cell.z + offset.z
            };
            if (!grid.Contains(candidate) || IsReserved(candidate))
            {
                continue;
            }

            const std::optional<std::vector<GridCell>> path =
                FindPath(worker->cell, candidate, workerId, world, grid);
            if (path.has_value() && (!foundWorkCell || path->size() < bestPath.size()))
            {
                foundWorkCell = true;
                workCell = candidate;
                bestPath = *path;
            }
        }

        if (!foundWorkCell)
        {
            return WorkOrderResult::NoReachableWorkCell;
        }

        const WorkPhase phase = bestPath.size() <= 1 ? WorkPhase::Working : WorkPhase::Moving;
        m_orders.push_back({
            WorkType::ChopTree,
            workerId,
            treeId,
            tree->cell,
            workCell,
            std::move(bestPath),
            1,
            phase
        });
        return WorkOrderResult::Accepted;
    }

    WorkOrderResult WorkSystem::AssignDigOrder(
        WorldObjectId workerId,
        GridCell target,
        World& world,
        const Grid& grid)
    {
        const WorldObject* worker = world.FindObject(workerId);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return WorkOrderResult::InvalidWorker;
        }
        if (!grid.Contains(target) || worker->cell.y != target.y)
        {
            return WorkOrderResult::InvalidTarget;
        }

        const Block* block = world.GetBlock(target);
        if (block == nullptr || block->type == BlockType::Air ||
            world.FindObjectAt(target).has_value())
        {
            return WorkOrderResult::InvalidTarget;
        }
        if (HasOrderForWorker(workerId))
        {
            return WorkOrderResult::WorkerBusy;
        }
        if (HasOrderForDigCell(target))
        {
            return WorkOrderResult::TargetBusy;
        }

        bool foundWorkCell = false;
        GridCell workCell{};
        std::vector<GridCell> bestPath;
        for (const GridCell offset : kAdjacentOffsets)
        {
            const GridCell candidate{
                target.x + offset.x,
                target.y + offset.y,
                target.z + offset.z
            };
            if (!grid.Contains(candidate) || IsReserved(candidate))
            {
                continue;
            }

            const std::optional<std::vector<GridCell>> path =
                FindPath(worker->cell, candidate, workerId, world, grid);
            if (path.has_value() && (!foundWorkCell || path->size() < bestPath.size()))
            {
                foundWorkCell = true;
                workCell = candidate;
                bestPath = *path;
            }
        }
        if (!foundWorkCell)
        {
            return WorkOrderResult::NoReachableWorkCell;
        }

        const WorkPhase phase = bestPath.size() <= 1 ? WorkPhase::Working : WorkPhase::Moving;
        m_orders.push_back({
            WorkType::DigBlock,
            workerId,
            std::nullopt,
            target,
            workCell,
            std::move(bestPath),
            1,
            phase
        });
        return WorkOrderResult::Accepted;
    }


    bool WorkSystem::HasOrderForWorker(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker)
            {
                return true;
            }
        }
        return false;
    }

    bool WorkSystem::HasOrderForTree(WorldObjectId tree) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.targetObject == tree)
            {
                return true;
            }
        }
        return false;
    }

    bool WorkSystem::HasOrderForDigCell(GridCell cell) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.type == WorkType::DigBlock &&
                order.targetCell.x == cell.x &&
                order.targetCell.y == cell.y &&
                order.targetCell.z == cell.z)
            {
                return true;
            }
        }
        return false;
    }


    bool WorkSystem::IsReserved(GridCell cell) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.workCell.x == cell.x && order.workCell.y == cell.y &&
                order.workCell.z == cell.z)
            {
                return true;
            }
        }
        return false;
    }

    std::optional<std::vector<GridCell>> WorkSystem::FindPath(
        GridCell start,
        GridCell goal,
        WorldObjectId worker,
        const World& world,
        const Grid& grid) const
    {
        return m_pathfinder.FindPath(start, goal, grid, [&](GridCell cell)
        {
            const std::optional<WorldObjectId> occupant = world.FindObjectAt(cell);
            const bool occupiedByAnotherObject = occupant.has_value() && *occupant != worker;
            const Block* block = world.GetBlock(cell);
            const WorldObject* workerObject = world.FindObject(worker);
            const bool isWorkersCurrentCell =
                workerObject != nullptr &&
                workerObject->cell.x == cell.x &&
                workerObject->cell.y == cell.y &&
                workerObject->cell.z == cell.z;
            const bool isWalkable = block != nullptr &&
                (block->type != BlockType::Air || isWorkersCurrentCell);
            const bool reservedForAnotherOrder =
                IsReserved(cell) && (cell.x != goal.x || cell.y != goal.y || cell.z != goal.z);
            return isWalkable && !occupiedByAnotherObject && !reservedForAnotherOrder;
        });
    }


}
