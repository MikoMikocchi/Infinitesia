#include "world/World.h"

#include <limits>
#include <stdexcept>

namespace Infinitesia
{
    WorldObjectSize GetWorldObjectSize(WorldObjectType type, float cellSize)
    {
        switch (type)
        {
        case WorldObjectType::Worker:
            return { cellSize * 0.62f, cellSize * 1.2f };
        case WorldObjectType::Tree:
            return { cellSize * 0.98f, cellSize * 1.95f };
        }

        throw std::invalid_argument("World object has an unsupported type.");
    }

    World::World(const Grid& grid, const std::vector<WorldObjectPlacement>& initialObjects)
    {
        m_objects.reserve(initialObjects.size());
        for (const WorldObjectPlacement& placement : initialObjects)
        {
            AddObject(placement.type, placement.cell, grid);
        }
    }

    WorldObjectId World::AddObject(WorldObjectType type, GridCell cell, const Grid& grid)
    {
        if (type != WorldObjectType::Worker && type != WorldObjectType::Tree)
        {
            throw std::invalid_argument("World object has an unsupported type.");
        }
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

        const WorldObjectId id{ m_nextObjectId++ };
        m_objects.push_back({ id, type, cell });
        return id;
    }

    std::optional<WorldObjectId> World::FindObjectAt(GridCell cell) const
    {
        for (const WorldObject& object : m_objects)
        {
            if (object.cell.column == cell.column && object.cell.row == cell.row)
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

    const std::vector<WorldObject>& World::GetObjects() const
    {
        return m_objects;
    }
}
