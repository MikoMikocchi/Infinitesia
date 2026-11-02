#include "world/World.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        void ValidateObjectState(WorldObjectType type, const std::optional<Item>& item)
        {
            switch (type)
            {
            case WorldObjectType::Worker:
            case WorldObjectType::Tree:
                if (item.has_value())
                {
                    throw std::invalid_argument("Non-item world objects cannot contain an item.");
                }
                return;
            case WorldObjectType::Item:
                if (!item.has_value())
                {
                    throw std::invalid_argument("World item is invalid.");
                }
                return;
            }
            throw std::invalid_argument("World object has an unsupported type.");
        }
    }

    WorldObjectSize GetWorldObjectSize(WorldObjectType type, float cellSize)
    {
        switch (type)
        {
        case WorldObjectType::Worker:
            return { cellSize * 0.62f, cellSize * 1.2f };
        case WorldObjectType::Tree:
            return { cellSize * 0.98f, cellSize * 1.95f };
        case WorldObjectType::Item:
            return { cellSize * 0.78f, cellSize * 0.36f };
        }

        throw std::invalid_argument("World object has an unsupported type.");
    }

    World::World(
        const Grid& grid,
        const std::vector<WorldObjectPlacement>& initialObjects,
        std::uint32_t seed)
        : m_seed(seed),
          m_gridWidth(grid.GetWidth()),
          m_gridHeight(grid.GetHeight()),
          m_gridDepth(grid.GetDepth())
    {
        m_objects.reserve(initialObjects.size());
        for (const WorldObjectPlacement& placement : initialObjects)
        {
            AddObject(placement.type, placement.cell, grid, placement.item);
        }
    }

    WorldObjectId World::AddObject(
        WorldObjectType type,
        GridCell cell,
        const Grid& grid,
        std::optional<Item> item)
    {
        ValidateObjectState(type, item);
        if (!grid.Contains(cell))
        {
            throw std::out_of_range("World object cell is outside the grid.");
        }
        EnsureChunkGenerated(GetChunkCoordinate(cell));
        if (FindObjectAt(cell).has_value())
        {
            throw std::invalid_argument("Grid cell is already occupied.");
        }
        if (m_nextObjectId == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("World object ID space is exhausted.");
        }

        const WorldObjectId id{ m_nextObjectId++ };
        m_objects.push_back({
            id,
            type,
            cell,
            grid.CellToSurface(cell),
            std::move(item),
            {}
        });
        return id;
    }

    WorldObjectId World::TransformObject(
        WorldObjectId id,
        WorldObjectType type,
        std::optional<Item> item)
    {
        ValidateObjectState(type, item);

        WorldObject* object = FindMutableObject(id);
        if (object == nullptr)
        {
            throw std::out_of_range("World object does not exist.");
        }
        if (m_nextObjectId == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("World object ID space is exhausted.");
        }

        object->id = { m_nextObjectId++ };
        object->type = type;
        object->item = std::move(item);
        return object->id;
    }

    bool World::RemoveObject(WorldObjectId id)
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

    bool World::TryAddToInventory(WorldObjectId workerId, const Item& item)
    {
        WorldObject* worker = FindMutableObject(workerId);
        return worker != nullptr &&
               worker->type == WorldObjectType::Worker &&
               worker->inventory.TryAdd(item);
    }

    bool World::SetObjectPosition(WorldObjectId id, WorldPosition position)
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            throw std::invalid_argument("World object position must be finite.");
        }

        WorldObject* object = FindMutableObject(id);
        if (object == nullptr)
        {
            return false;
        }
        object->position = position;
        return true;
    }

    bool World::SetObjectCell(WorldObjectId id, GridCell cell, const Grid& grid)
    {
        if (!grid.Contains(cell))
        {
            throw std::out_of_range("World object cell is outside the grid.");
        }
        for (const WorldObject& object : m_objects)
        {
            if (object.id != id && object.cell.x == cell.x &&
                object.cell.y == cell.y && object.cell.z == cell.z)
            {
                return false;
            }
        }

        WorldObject* object = FindMutableObject(id);
        if (object == nullptr)
        {
            return false;
        }
        object->cell = cell;
        object->position = grid.CellToSurface(cell);
        return true;
    }

    std::optional<WorldObjectId> World::FindObjectAt(GridCell cell) const
    {
        for (const WorldObject& object : m_objects)
        {
            if (object.cell.x == cell.x && object.cell.y == cell.y && object.cell.z == cell.z)
            {
                return object.id;
            }
        }

        return std::nullopt;
    }

    const WorldObject* World::FindObject(WorldObjectId id) const
    {
        for (const WorldObject& object : m_objects)
        {
            if (object.id == id)
            {
                return &object;
            }
        }

        return nullptr;
    }

    WorldObject* World::FindMutableObject(WorldObjectId id)
    {
        for (WorldObject& object : m_objects)
        {
            if (object.id == id)
            {
                return &object;
            }
        }
        return nullptr;
    }

    const std::vector<WorldObject>& World::GetObjects() const
    {
        return m_objects;
    }
}
