#include "world/WorkSystem.h"
#include "world/work/WorkPathfinding.h"
#include "world/work/WorkUtils.h"

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
        return WorkInternal::HasOrderForWorker(m_orders, worker);
    }

    bool WorkSystem::HasOrderForTree(WorldObjectId tree) const
    {
        return WorkInternal::HasOrderForTree(m_orders, tree);
    }

    bool WorkSystem::HasOrderForDigCell(GridCell cell) const
    {
        return WorkInternal::HasOrderForDigCell(m_orders, cell);
    }

    bool WorkSystem::IsReserved(GridCell cell) const
    {
        return WorkInternal::IsReserved(m_orders, cell);
    }

    std::optional<std::vector<GridCell>> WorkSystem::FindPath(
        GridCell start,
        GridCell goal,
        WorldObjectId worker,
        const World& world,
        const Grid& grid) const
    {
        return WorkInternal::FindPath(const_cast<GridPathfinder&>(m_pathfinder), start, goal, worker, world, grid, m_orders);
    }

    float WorkSystem::GetWorkDuration(const WorkOrder& order) const
    {
        return WorkInternal::GetWorkDuration(m_config, order);
    }
}
