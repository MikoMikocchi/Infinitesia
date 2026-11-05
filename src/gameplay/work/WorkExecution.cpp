#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "gameplay/work/TreeFelling.h"
#include "gameplay/work/WorkMovement.h"
#include "gameplay/work/WorkSystem.h"
#include "gameplay/work/WorkUtils.h"

namespace Infinitesia
{
void WorkSystem::Tick(GameWorld &world, const Grid &grid, float fixedDeltaTime)
{
	if (!std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f)
	{
		throw std::invalid_argument("Work system time step must be finite and greater than zero.");
	}

	for (std::size_t index = 0; index < m_orders.size();)
	{
		const bool orderIsValid = WorkInternal::AdvanceOrder(*this, m_orders[index], world, grid, fixedDeltaTime);
		const WorldObject *worker = world.entities.FindObject(m_orders[index].worker);
		const Block *targetBlock =
		    m_orders[index].type == WorkType::DigBlock ? world.terrain.GetBlock(m_orders[index].targetCell) : nullptr;
		if (!orderIsValid || worker == nullptr || !worker->Is(ObjectAbility::Worker) ||
		    (m_orders[index].type == WorkType::ChopTree && !IsTreeWood(world.terrain, m_orders[index].targetCell)) ||
		    (m_orders[index].type == WorkType::DigBlock &&
		     (targetBlock == nullptr || !world.terrain.Registry().Diggable(targetBlock->state))))
		{
			m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
			continue;
		}

		if (m_orders[index].type == WorkType::MoveToCell && m_orders[index].phase == WorkPhase::Working)
		{
			m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
			continue;
		}

		if (m_orders[index].phase == WorkPhase::Working &&
		    m_orders[index].workProgress >= GetWorkDuration(m_orders[index]))
		{
			if (m_orders[index].type == WorkType::ChopTree)
			{
				FellAndDropLogs(world, grid, m_orders[index].targetCell);
			}
			else
			{
				world.terrain.SetBlock(m_orders[index].targetCell, BlockState{kAirMatter, kShapeFull});
			}
			m_orders.erase(m_orders.begin() + static_cast<std::ptrdiff_t>(index));
			continue;
		}

		++index;
	}
}

void WorkSystem::FellAndDropLogs(GameWorld &world, const Grid &grid, GridCell treeCell)
{
	const FelledTree felled = FellTree(world.terrain, treeCell);
	if (felled.wood.empty() || m_drop.pile == nullptr || m_drop.logs == nullptr)
	{
		return;
	}

	// The logs lie on the stump: ground that is free and open above, lowest first
	// (the cell of a root at the surface, where the trunk stood).
	std::vector<GridCell> places = felled.wood;
	std::sort(places.begin(), places.end(), [](const GridCell &a, const GridCell &b) { return a.y < b.y; });
	const BlockRegistry &registry = world.terrain.Registry();
	int logs = std::max(1, static_cast<int>(std::lround(felled.woodKilograms / m_config.kilogramsPerLog)));
	for (const GridCell &cell : places)
	{
		if (logs <= 0)
		{
			break;
		}
		const Block *ground = world.terrain.GetBlock(cell);
		const Block *above = world.terrain.GetBlock({cell.x, cell.y + 1, cell.z});
		if (ground == nullptr || !registry.Walkable(ground->state) ||
		    (above != nullptr && registry.Occupied(above->state)) || world.entities.FindObjectAt(cell).has_value())
		{
			continue;
		}
		const int pile = std::min(logs, m_drop.logs->maxStack);
		world.AddObject(*m_drop.pile, cell, grid, Item{*m_drop.logs, pile});
		logs -= pile;
	}
}

WorkerActivity WorkSystem::GetActivity(WorldObjectId worker) const
{
	for (const WorkOrder &order : m_orders)
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
	for (const WorkOrder &order : m_orders)
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
	for (const WorkOrder &order : m_orders)
	{
		if (order.worker == worker && order.phase == WorkPhase::Moving && order.nextPathCell < order.path.size())
		{
			return {order.path.begin() + static_cast<std::ptrdiff_t>(order.nextPathCell), order.path.end()};
		}
	}
	return {};
}
} // namespace Infinitesia
