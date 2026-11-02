#include "world/WorkSystem.h"

#include <array>
#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr std::array<GridCell, 4> kAdjacentOffsets{
            GridCell{ 0, -1 },
            GridCell{ 1, 0 },
            GridCell{ 0, 1 },
            GridCell{ -1, 0 }
        };
    }

    WorkSystem::WorkSystem(WorkerConfig config)
        : m_config(config)
    {
        if (!std::isfinite(config.movementSpeed) || config.movementSpeed <= 0.0f ||
            !std::isfinite(config.choppingDuration) || config.choppingDuration <= 0.0f ||
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
        int bestDistance = 0;
        for (const GridCell offset : kAdjacentOffsets)
        {
            const GridCell candidate{ tree->cell.column + offset.column, tree->cell.row + offset.row };
            if (!grid.Contains(candidate) || world.FindObjectAt(candidate).has_value() || IsReserved(candidate))
            {
                continue;
            }

            const int distance = std::abs(worker->cell.column - candidate.column) +
                                 std::abs(worker->cell.row - candidate.row);
            if (!foundWorkCell || distance < bestDistance)
            {
                foundWorkCell = true;
                workCell = candidate;
                bestDistance = distance;
            }
        }

        if (!foundWorkCell)
        {
            return WorkOrderResult::NoReachableWorkCell;
        }

        m_orders.push_back({ workerId, treeId, workCell, WorkPhase::Moving });
        return WorkOrderResult::Accepted;
    }

    void WorkSystem::Tick(World& world, const Grid& grid, float fixedDeltaTime)
    {
        if (!std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
        {
            throw std::invalid_argument("Work system time step must be finite and greater than zero.");
        }

        for (std::size_t index = 0; index < m_orders.size();)
        {
            AdvanceOrder(m_orders[index], world, grid, fixedDeltaTime);
            const WorldObject* worker = world.FindObject(m_orders[index].worker);
            const WorldObject* tree = world.FindObject(m_orders[index].tree);
            if (worker == nullptr || worker->type != WorldObjectType::Worker ||
                tree == nullptr || tree->type != WorldObjectType::Tree)
            {
                m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            if (m_orders[index].phase == WorkPhase::Chopping &&
                m_orders[index].choppingProgress >= m_config.choppingDuration)
            {
                world.TransformObject(m_orders[index].tree, WorldObjectType::Logs, m_config.logsPerTree);
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
                return order.phase == WorkPhase::Moving ? WorkerActivity::Moving : WorkerActivity::Working;
            }
        }
        return WorkerActivity::Idle;
    }

    float WorkSystem::GetWorkProgress(WorldObjectId worker) const
    {
        for (const WorkOrder& order : m_orders)
        {
            if (order.worker == worker && order.phase == WorkPhase::Chopping)
            {
                return order.choppingProgress / m_config.choppingDuration;
            }
        }
        return 0.0f;
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
            if (order.tree == tree)
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
            if (order.workCell.column == cell.column && order.workCell.row == cell.row)
            {
                return true;
            }
        }
        return false;
    }

    void WorkSystem::AdvanceOrder(WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime)
    {
        const WorldObject* worker = world.FindObject(order.worker);
        const WorldObject* tree = world.FindObject(order.tree);
        if (worker == nullptr || tree == nullptr)
        {
            return;
        }

        if (order.phase == WorkPhase::Moving)
        {
            const WorldPosition destination = grid.CellToWorld(order.workCell);
            const float deltaX = destination.x - worker->position.x;
            const float deltaZ = destination.z - worker->position.z;
            const float distance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
            const float step = m_config.movementSpeed * fixedDeltaTime;

            if (distance <= step)
            {
                if (!world.SetObjectCell(order.worker, order.workCell, grid))
                {
                    return;
                }
                order.phase = WorkPhase::Chopping;
                return;
            }

            const float movementRatio = step / distance;
            world.SetObjectPosition(
                order.worker,
                {
                    worker->position.x + deltaX * movementRatio,
                    worker->position.y,
                    worker->position.z + deltaZ * movementRatio
                });
            return;
        }

        order.choppingProgress += fixedDeltaTime;
    }
}
