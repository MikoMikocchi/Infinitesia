#include "world/WorkSystem.h"
#include "world/work/WorkPathfinding.h"
#include "world/work/WorkUtils.h"

#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
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
