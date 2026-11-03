#pragma once

#include "gameplay/entities/WorldObject.h"
#include "world/Grid.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Infinitesia
{
    // The things standing in the world (workers, trees, items on the ground). Positions are
    // grid cells; the store does not know about terrain, so a caller that needs the chunk
    // under a new object generated asks the VoxelWorld itself (GameWorld::AddObject does).
    class EntityStore
    {
    public:
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
        bool TryAddToInventory(WorldObjectId worker, const Item& item);
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
