#include "gameplay/GameWorld.h"

#include <utility>

namespace Infinitesia
{
    GameWorld::GameWorld(
        const Grid& grid,
        const std::vector<WorldObjectPlacement>& initialObjects,
        std::uint32_t seed)
        : terrain(grid, seed)
    {
        for (const WorldObjectPlacement& placement : initialObjects)
        {
            AddObject(placement.type, placement.cell, grid, placement.item);
        }
    }

    GameWorld::GameWorld(
        const Grid& grid,
        const std::vector<WorldObjectPlacement>& initialObjects,
        std::shared_ptr<const TerrainSource> source)
        : terrain(grid, std::move(source))
    {
        for (const WorldObjectPlacement& placement : initialObjects)
        {
            AddObject(placement.type, placement.cell, grid, placement.item);
        }
    }

    WorldObjectId GameWorld::AddObject(
        WorldObjectType type,
        GridCell cell,
        const Grid& grid,
        std::optional<Item> item)
    {
        if (grid.Contains(cell))
        {
            terrain.EnsureChunkContaining(cell);
        }
        return entities.AddObject(type, cell, grid, std::move(item));
    }
}
