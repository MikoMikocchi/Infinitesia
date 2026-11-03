#include "gameplay/work/WorkSystem.h"

#include <array>
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

    WorkOrderResult WorkSystem::AssignChopOrder(
        WorldObjectId workerId,
        WorldObjectId treeId,
        GameWorld& world,
        const Grid& grid)
    {
        const WorldObject* worker = world.entities.FindObject(workerId);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return WorkOrderResult::InvalidWorker;
        }

        const WorldObject* tree = world.entities.FindObject(treeId);
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
        GameWorld& world,
        const Grid& grid)
    {
        const WorldObject* worker = world.entities.FindObject(workerId);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return WorkOrderResult::InvalidWorker;
        }
        if (!grid.Contains(target) || worker->cell.y != target.y)
        {
            return WorkOrderResult::InvalidTarget;
        }

        const Block* block = world.terrain.GetBlock(target);
        if (block == nullptr || block->type == BlockType::Air ||
            world.entities.FindObjectAt(target).has_value())
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

    WorkOrderResult WorkSystem::AssignMoveOrder(
        WorldObjectId workerId,
        GridCell target,
        GameWorld& world,
        const Grid& grid)
    {
        const WorldObject* worker = world.entities.FindObject(workerId);
        if (worker == nullptr || worker->type != WorldObjectType::Worker)
        {
            return WorkOrderResult::InvalidWorker;
        }
        if (!grid.Contains(target) || worker->cell.y != target.y)
        {
            return WorkOrderResult::InvalidTarget;
        }

        const Block* block = world.terrain.GetBlock(target);
        if (block == nullptr || block->type == BlockType::Air)
        {
            return WorkOrderResult::InvalidTarget;
        }
        const std::optional<WorldObjectId> occupant = world.entities.FindObjectAt(target);
        if (occupant.has_value() && *occupant != workerId)
        {
            return WorkOrderResult::InvalidTarget;
        }
        if (HasOrderForWorker(workerId))
        {
            return WorkOrderResult::WorkerBusy;
        }
        if (IsReserved(target))
        {
            return WorkOrderResult::TargetBusy;
        }

        const std::optional<std::vector<GridCell>> path =
            FindPath(worker->cell, target, workerId, world, grid);
        if (!path.has_value())
        {
            return WorkOrderResult::NoReachableWorkCell;
        }

        m_orders.push_back({
            WorkType::MoveToCell,
            workerId,
            std::nullopt,
            target,
            target,
            *path,
            1,
            path->size() <= 1 ? WorkPhase::Working : WorkPhase::Moving
        });
        return WorkOrderResult::Accepted;
    }
}
