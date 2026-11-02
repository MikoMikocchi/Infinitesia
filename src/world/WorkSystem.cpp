#include "world/WorkSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

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

        struct OpenPathNode
        {
            std::size_t index;
            int pathCost;
            int estimatedTotalCost;
        };

        struct OpenPathNodeOrder
        {
            bool operator()(const OpenPathNode& left, const OpenPathNode& right) const
            {
                if (left.estimatedTotalCost != right.estimatedTotalCost)
                {
                    return left.estimatedTotalCost > right.estimatedTotalCost;
                }
                return left.pathCost > right.pathCost;
            }
        };

        int ManhattanDistance(GridCell from, GridCell to)
        {
            return std::abs(from.column - to.column) + std::abs(from.row - to.row);
        }
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
        std::vector<GridCell> bestPath;
        for (const GridCell offset : kAdjacentOffsets)
        {
            const GridCell candidate{ tree->cell.column + offset.column, tree->cell.row + offset.row };
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

        const WorkPhase phase = bestPath.size() <= 1 ? WorkPhase::Chopping : WorkPhase::Moving;
        m_orders.push_back({ workerId, treeId, workCell, std::move(bestPath), 1, phase });
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
            const bool orderIsValid = AdvanceOrder(m_orders[index], world, grid, fixedDeltaTime);
            const WorldObject* worker = world.FindObject(m_orders[index].worker);
            const WorldObject* tree = world.FindObject(m_orders[index].tree);
            if (!orderIsValid || worker == nullptr || worker->type != WorldObjectType::Worker ||
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

    std::optional<std::vector<GridCell>> WorkSystem::FindPath(
        GridCell start,
        GridCell goal,
        WorldObjectId worker,
        const World& world,
        const Grid& grid) const
    {
        if (!grid.Contains(start) || !grid.Contains(goal))
        {
            return std::nullopt;
        }

        const std::size_t columns = static_cast<std::size_t>(grid.GetColumns());
        const std::size_t cellCount = columns * static_cast<std::size_t>(grid.GetRows());
        const auto cellIndex = [columns](GridCell cell)
        {
            return static_cast<std::size_t>(cell.row) * columns + static_cast<std::size_t>(cell.column);
        };
        const auto indexCell = [columns](std::size_t index)
        {
            return GridCell{
                static_cast<int>(index % columns),
                static_cast<int>(index / columns)
            };
        };

        const std::size_t startIndex = cellIndex(start);
        const std::size_t goalIndex = cellIndex(goal);
        std::vector<int> pathCosts(cellCount, std::numeric_limits<int>::max());
        std::vector<std::size_t> parents(cellCount, cellCount);
        std::priority_queue<OpenPathNode, std::vector<OpenPathNode>, OpenPathNodeOrder> openNodes;
        pathCosts[startIndex] = 0;
        openNodes.push({ startIndex, 0, ManhattanDistance(start, goal) });

        constexpr std::array<GridCell, 4> neighborOffsets{
            GridCell{ 0, -1 },
            GridCell{ 1, 0 },
            GridCell{ 0, 1 },
            GridCell{ -1, 0 }
        };

        while (!openNodes.empty())
        {
            const OpenPathNode current = openNodes.top();
            openNodes.pop();
            if (current.pathCost != pathCosts[current.index])
            {
                continue;
            }
            if (current.index == goalIndex)
            {
                break;
            }

            const GridCell currentCell = indexCell(current.index);
            for (const GridCell offset : neighborOffsets)
            {
                const GridCell neighbor{
                    currentCell.column + offset.column,
                    currentCell.row + offset.row
                };
                if (!grid.Contains(neighbor))
                {
                    continue;
                }

                const std::optional<WorldObjectId> occupant = world.FindObjectAt(neighbor);
                if ((occupant.has_value() && *occupant != worker) ||
                    (IsReserved(neighbor) && (neighbor.column != goal.column || neighbor.row != goal.row)))
                {
                    continue;
                }

                const std::size_t neighborIndex = cellIndex(neighbor);
                const int nextCost = current.pathCost + 1;
                if (nextCost >= pathCosts[neighborIndex])
                {
                    continue;
                }

                pathCosts[neighborIndex] = nextCost;
                parents[neighborIndex] = current.index;
                openNodes.push({
                    neighborIndex,
                    nextCost,
                    nextCost + ManhattanDistance(neighbor, goal)
                });
            }
        }

        if (pathCosts[goalIndex] == std::numeric_limits<int>::max())
        {
            return std::nullopt;
        }

        std::vector<GridCell> path;
        for (std::size_t current = goalIndex; current != cellCount; current = parents[current])
        {
            path.push_back(indexCell(current));
            if (current == startIndex)
            {
                break;
            }
        }
        if (path.empty() || path.back().column != start.column || path.back().row != start.row)
        {
            return std::nullopt;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }

    bool WorkSystem::AdvanceOrder(WorkOrder& order, World& world, const Grid& grid, float fixedDeltaTime)
    {
        const WorldObject* worker = world.FindObject(order.worker);
        const WorldObject* tree = world.FindObject(order.tree);
        if (worker == nullptr || tree == nullptr)
        {
            return false;
        }

        if (order.phase == WorkPhase::Moving)
        {
            float remainingMovement = m_config.movementSpeed * fixedDeltaTime;
            while (remainingMovement > 0.0f && order.nextPathCell < order.path.size())
            {
                worker = world.FindObject(order.worker);
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

                const WorldPosition destination = grid.CellToWorld(nextCell);
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
                order.phase = WorkPhase::Chopping;
            }
            return true;
        }

        order.choppingProgress += fixedDeltaTime;
        return true;
    }
}
