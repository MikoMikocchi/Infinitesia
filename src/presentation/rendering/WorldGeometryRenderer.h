#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "gameplay/entities/EntityStore.h"
#include "world/VoxelWorld.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"

#include <cstdint>
#include <map>
#include <utility>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        struct ChunkMeshCacheEntry
        {
            Mesh mesh{};
            std::uint64_t terrainRevision = 0;
        };

        using ChunkMeshKey = std::pair<ChunkCoordinate, int>;
        using ChunkMeshCache = std::map<ChunkMeshKey, ChunkMeshCacheEntry>;

        void DrawBlocks(
            const Grid& grid,
            const VoxelWorld& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel,
            Material material,
            const BlockTextureAtlas& atlas,
            ChunkMeshCache& meshCache);
        void DrawObjectShadows(
            const Grid& grid,
            const EntityStore& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel);
        void DrawObjects(
            const Grid& grid,
            const EntityStore& world,
            Mesh cylinderMesh,
            Mesh sphereMesh,
            Material material,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel);
    }
}
