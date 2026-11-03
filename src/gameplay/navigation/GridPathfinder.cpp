#include "gameplay/navigation/GridPathfinder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace Infinitesia
{
    namespace
    {
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
            return std::abs(from.x - to.x) + std::abs(from.y - to.y) +
                   std::abs(from.z - to.z);
        }
    }

    std::optional<std::vector<GridCell>> GridPathfinder::FindPath(
        GridCell start,
        GridCell goal,
        const Grid& grid,
        const CellPassability& isCellPassable) const
    {
        if (!grid.Contains(start) || !grid.Contains(goal))
        {
            return std::nullopt;
        }

        const std::size_t width = static_cast<std::size_t>(grid.GetWidth());
        const std::size_t height = static_cast<std::size_t>(grid.GetHeight());
        const auto cellIndex = [width, height](GridCell cell)
        {
            return (static_cast<std::size_t>(cell.z) * height +
                    static_cast<std::size_t>(cell.y)) * width +
                   static_cast<std::size_t>(cell.x);
        };
        const auto indexCell = [width, height](std::size_t index)
        {
            return GridCell{
                static_cast<int>(index % width),
                static_cast<int>((index / width) % height),
                static_cast<int>(index / (width * height))
            };
        };

        const std::size_t startIndex = cellIndex(start);
        const std::size_t goalIndex = cellIndex(goal);
        std::unordered_map<std::size_t, int> pathCosts;
        std::unordered_map<std::size_t, std::size_t> parents;
        std::priority_queue<OpenPathNode, std::vector<OpenPathNode>, OpenPathNodeOrder> openNodes;
        pathCosts.emplace(startIndex, 0);
        openNodes.push({ startIndex, 0, ManhattanDistance(start, goal) });

        constexpr std::array<GridCell, 4> neighborOffsets{
            GridCell{ 0, 0, -1 },
            GridCell{ 1, 0, 0 },
            GridCell{ 0, 0, 1 },
            GridCell{ -1, 0, 0 }
        };

        while (!openNodes.empty())
        {
            const OpenPathNode current = openNodes.top();
            openNodes.pop();
            const auto currentCost = pathCosts.find(current.index);
            if (currentCost == pathCosts.end() || current.pathCost != currentCost->second)
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
                    currentCell.x + offset.x,
                    currentCell.y + offset.y,
                    currentCell.z + offset.z
                };
                if (!grid.Contains(neighbor) || !isCellPassable(neighbor))
                {
                    continue;
                }

                const std::size_t neighborIndex = cellIndex(neighbor);
                const int nextCost = current.pathCost + 1;
                const auto neighborCost = pathCosts.find(neighborIndex);
                if (neighborCost != pathCosts.end() && nextCost >= neighborCost->second)
                {
                    continue;
                }

                pathCosts.insert_or_assign(neighborIndex, nextCost);
                parents.insert_or_assign(neighborIndex, current.index);
                openNodes.push({
                    neighborIndex,
                    nextCost,
                    nextCost + ManhattanDistance(neighbor, goal)
                });
            }
        }

        if (pathCosts.find(goalIndex) == pathCosts.end())
        {
            return std::nullopt;
        }

        std::vector<GridCell> path;
        for (std::size_t current = goalIndex;;)
        {
            path.push_back(indexCell(current));
            if (current == startIndex)
            {
                break;
            }
            const auto parent = parents.find(current);
            if (parent == parents.end())
            {
                return std::nullopt;
            }
            current = parent->second;
        }
        std::reverse(path.begin(), path.end());
        return path;
    }
}
