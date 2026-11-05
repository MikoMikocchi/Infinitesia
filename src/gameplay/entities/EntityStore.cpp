#include "gameplay/entities/EntityStore.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
namespace
{
void ValidateObjectState(const ObjectKindDef &kind, const std::optional<Item> &item)
{
	if (kind.carriesItem && !item.has_value())
	{
		throw std::invalid_argument("World item is invalid.");
	}
	if (!kind.carriesItem && item.has_value())
	{
		throw std::invalid_argument("Non-item world objects cannot contain an item.");
	}
}
} // namespace

WorldObjectSize GetWorldObjectSize(const ObjectKindDef &kind, float cellSize)
{
	return {cellSize * kind.widthInCells, cellSize * kind.heightInCells};
}

WorldObjectId EntityStore::AddObject(const ObjectKindDef &kind, GridCell cell, const Grid &grid,
                                     std::optional<Item> item)
{
	ValidateObjectState(kind, item);
	if (!grid.Contains(cell))
	{
		throw std::out_of_range("World object cell is outside the grid.");
	}
	if (FindObjectAt(cell).has_value())
	{
		throw std::invalid_argument("Grid cell is already occupied.");
	}
	if (m_nextObjectId == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::overflow_error("World object ID space is exhausted.");
	}

	const WorldObjectId id{m_nextObjectId++};
	m_objects.push_back({id, &kind, cell, grid.CellToSurface(cell), std::move(item), {}});
	return id;
}

WorldObjectId EntityStore::TransformObject(WorldObjectId id, const ObjectKindDef &kind, std::optional<Item> item)
{
	ValidateObjectState(kind, item);

	WorldObject *object = FindMutableObject(id);
	if (object == nullptr)
	{
		throw std::out_of_range("World object does not exist.");
	}
	if (m_nextObjectId == std::numeric_limits<std::uint64_t>::max())
	{
		throw std::overflow_error("World object ID space is exhausted.");
	}

	object->id = {m_nextObjectId++};
	object->kind = &kind;
	object->item = std::move(item);
	return object->id;
}

bool EntityStore::RemoveObject(WorldObjectId id)
{
	for (auto object = m_objects.begin(); object != m_objects.end(); ++object)
	{
		if (object->id == id)
		{
			m_objects.erase(object);
			return true;
		}
	}
	return false;
}

bool EntityStore::TryAddToInventory(WorldObjectId workerId, const Item &item)
{
	WorldObject *worker = FindMutableObject(workerId);
	return worker != nullptr && worker->Is(ObjectAbility::Worker) && worker->inventory.TryAdd(item);
}

bool EntityStore::SetObjectPosition(WorldObjectId id, WorldPosition position)
{
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
	{
		throw std::invalid_argument("World object position must be finite.");
	}

	WorldObject *object = FindMutableObject(id);
	if (object == nullptr)
	{
		return false;
	}
	object->position = position;
	return true;
}

bool EntityStore::SetObjectCell(WorldObjectId id, GridCell cell, const Grid &grid)
{
	if (!grid.Contains(cell))
	{
		throw std::out_of_range("World object cell is outside the grid.");
	}
	for (const WorldObject &object : m_objects)
	{
		if (object.id != id && object.cell.x == cell.x && object.cell.y == cell.y && object.cell.z == cell.z)
		{
			return false;
		}
	}

	WorldObject *object = FindMutableObject(id);
	if (object == nullptr)
	{
		return false;
	}
	object->cell = cell;
	object->position = grid.CellToSurface(cell);
	return true;
}

std::optional<WorldObjectId> EntityStore::FindObjectAt(GridCell cell) const
{
	for (const WorldObject &object : m_objects)
	{
		if (object.cell.x == cell.x && object.cell.y == cell.y && object.cell.z == cell.z)
		{
			return object.id;
		}
	}

	return std::nullopt;
}

const WorldObject *EntityStore::FindObject(WorldObjectId id) const
{
	for (const WorldObject &object : m_objects)
	{
		if (object.id == id)
		{
			return &object;
		}
	}

	return nullptr;
}

WorldObject *EntityStore::FindMutableObject(WorldObjectId id)
{
	for (WorldObject &object : m_objects)
	{
		if (object.id == id)
		{
			return &object;
		}
	}
	return nullptr;
}

const std::vector<WorldObject> &EntityStore::GetObjects() const
{
	return m_objects;
}
} // namespace Infinitesia
