#pragma once

#include "world/Grid.h"
#include "world/Item.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Infinitesia
{
    enum class WorldObjectType
    {
        Worker,
        Tree,
        Item
    };

    struct WorldObjectId
    {
        std::uint64_t value;

        bool operator==(const WorldObjectId&) const = default;
    };

    struct WorldObjectPlacement
    {
        WorldObjectType type;
        GridCell cell;
        std::optional<Item> item = std::nullopt;
    };

    struct WorldObject
    {
        WorldObjectId id;
        WorldObjectType type;
        GridCell cell;
        WorldPosition position;
        std::optional<Item> item;
    };

    struct WorldObjectSize
    {
        float width;
        float height;
    };

    WorldObjectSize GetWorldObjectSize(WorldObjectType type, float cellSize);

    class World
    {
    public:
        World(const Grid& grid, const std::vector<WorldObjectPlacement>& initialObjects = {});

        WorldObjectId AddObject(
            WorldObjectType type,
            GridCell cell,
            const Grid& grid,
            std::optional<Item> item = std::nullopt);
        WorldObjectId TransformObject(
            WorldObjectId id,
            WorldObjectType type,
            std::optional<Item> item = std::nullopt);
        bool RemoveObject(WorldObjectId id);
        bool SetObjectPosition(WorldObjectId id, WorldPosition position);
        bool SetObjectCell(WorldObjectId id, GridCell cell, const Grid& grid);
        std::optional<WorldObjectId> FindObjectAt(GridCell cell) const;
        const WorldObject* FindObject(WorldObjectId id) const;
        const std::vector<WorldObject>& GetObjects() const;

    private:
        WorldObject* FindMutableObject(WorldObjectId id);

        std::vector<WorldObject> m_objects;
        std::uint64_t m_nextObjectId = 1;
    };
}
