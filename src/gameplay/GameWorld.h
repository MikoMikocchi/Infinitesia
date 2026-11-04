#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "gameplay/entities/EntityStore.h"
#include "world/VoxelWorld.h"

namespace Infinitesia {
// The terrain together with what stands on it: what the work system and the
// game loop act on. Systems that need only one half take that half (VoxelWorld
// or EntityStore).
struct GameWorld {
  GameWorld(const Grid& grid,
            const std::vector<WorldObjectPlacement>& initialObjects = {},
            std::uint32_t seed = 1);
  GameWorld(const Grid& grid,
            const std::vector<WorldObjectPlacement>& initialObjects,
            std::shared_ptr<const TerrainSource> terrain);

  // Adds an object and makes sure the terrain under it exists.
  WorldObjectId AddObject(const ObjectKindDef& kind, GridCell cell,
                          const Grid& grid,
                          std::optional<Item> item = std::nullopt);

  VoxelWorld terrain;
  EntityStore entities;
};
}  // namespace Infinitesia
