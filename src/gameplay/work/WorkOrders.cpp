#include <algorithm>
#include <array>
#include <utility>

#include "gameplay/work/TreeFelling.h"
#include "gameplay/work/WorkSystem.h"

namespace Infinitesia
{
namespace
{
constexpr std::array<GridCell, 4> kAdjacentOffsets{GridCell{0, 0, -1}, GridCell{1, 0, 0}, GridCell{0, 0, 1},
                                                   GridCell{-1, 0, 0}};
}

WorkOrderResult WorkSystem::AssignChopOrder(WorldObjectId workerId, GridCell treeCell, GameWorld &world,
                                            const Grid &grid)
{
	const WorldObject *worker = world.entities.FindObject(workerId);
	if (worker == nullptr || !worker->Is(ObjectAbility::Worker))
	{
		return WorkOrderResult::InvalidWorker;
	}

	const std::vector<GridCell> wood = FindTreeWood(world.terrain, treeCell);
	if (wood.empty())
	{
		return WorkOrderResult::InvalidTarget;
	}
	if (HasOrderForWorker(workerId))
	{
		return WorkOrderResult::WorkerBusy;
	}
	if (HasOrderForTree(wood))
	{
		return WorkOrderResult::TargetBusy;
	}

	// The worker stands on the ground beside the trunk: the cell of ground one
	// level under a cell of wood, next to it on one of the four sides. A cell of
	// wood high in the crown has only air under it.
	constexpr std::size_t kMaxWorkCells = 64;
	std::vector<GridCell> candidates;
	for (const GridCell &cell : wood)
	{
		for (const GridCell offset : kAdjacentOffsets)
		{
			const GridCell candidate{cell.x + offset.x, cell.y - 1, cell.z + offset.z};
			const Block *block = grid.Contains(candidate) ? world.terrain.GetBlock(candidate) : nullptr;
			if (block == nullptr || !world.terrain.Registry().Walkable(block->state) || IsReserved(candidate) ||
			    world.entities.FindObjectAt(candidate).has_value())
			{
				continue;
			}
			if (std::none_of(candidates.begin(), candidates.end(), [&](const GridCell &known) {
				    return known.x == candidate.x && known.y == candidate.y && known.z == candidate.z;
			    }))
			{
				candidates.push_back(candidate);
			}
		}
		if (candidates.size() >= kMaxWorkCells)
		{
			break;
		}
	}

	bool foundWorkCell = false;
	GridCell workCell{};
	std::vector<GridCell> bestPath;
	for (const GridCell &candidate : candidates)
	{
		const std::optional<std::vector<GridCell>> path = FindPath(worker->cell, candidate, workerId, world, grid);
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
	m_orders.push_back({WorkType::ChopTree, workerId, std::nullopt, treeCell, workCell, std::move(bestPath), 1, phase});
	return WorkOrderResult::Accepted;
}

WorkOrderResult WorkSystem::AssignDigOrder(WorldObjectId workerId, GridCell target, GameWorld &world, const Grid &grid)
{
	const WorldObject *worker = world.entities.FindObject(workerId);
	if (worker == nullptr || !worker->Is(ObjectAbility::Worker))
	{
		return WorkOrderResult::InvalidWorker;
	}
	if (!grid.Contains(target) || worker->cell.y != target.y)
	{
		return WorkOrderResult::InvalidTarget;
	}

	const Block *block = world.terrain.GetBlock(target);
	if (block == nullptr || !world.terrain.Registry().Diggable(block->state) ||
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
		const GridCell candidate{target.x + offset.x, target.y + offset.y, target.z + offset.z};
		if (!grid.Contains(candidate) || IsReserved(candidate))
		{
			continue;
		}

		const std::optional<std::vector<GridCell>> path = FindPath(worker->cell, candidate, workerId, world, grid);
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
	m_orders.push_back({WorkType::DigBlock, workerId, std::nullopt, target, workCell, std::move(bestPath), 1, phase});
	return WorkOrderResult::Accepted;
}

WorkOrderResult WorkSystem::AssignMoveOrder(WorldObjectId workerId, GridCell target, GameWorld &world, const Grid &grid)
{
	const WorldObject *worker = world.entities.FindObject(workerId);
	if (worker == nullptr || !worker->Is(ObjectAbility::Worker))
	{
		return WorkOrderResult::InvalidWorker;
	}
	if (!grid.Contains(target) || worker->cell.y != target.y)
	{
		return WorkOrderResult::InvalidTarget;
	}

	const Block *block = world.terrain.GetBlock(target);
	if (block == nullptr || !world.terrain.Registry().Walkable(block->state))
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

	const std::optional<std::vector<GridCell>> path = FindPath(worker->cell, target, workerId, world, grid);
	if (!path.has_value())
	{
		return WorkOrderResult::NoReachableWorkCell;
	}

	m_orders.push_back({WorkType::MoveToCell, workerId, std::nullopt, target, target, *path, 1,
	                    path->size() <= 1 ? WorkPhase::Working : WorkPhase::Moving});
	return WorkOrderResult::Accepted;
}
} // namespace Infinitesia
