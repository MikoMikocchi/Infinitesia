#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"

#include <cstdint>
#include <map>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        struct ChunkMeshCacheEntry
        {
            Mesh mesh{};
            std::uint64_t terrainRevision = 0;
            std::uint64_t worldTerrainRevision = 0;
            int level = -1;
        };

        using ChunkMeshCache = std::map<ChunkCoordinate, ChunkMeshCacheEntry>;

        void DrawBlocks(
            const Grid& grid,
            const World& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel,
            Material material,
            ChunkMeshCache& meshCache);
        void DrawObjectShadows(
            const Grid& grid,
            const World& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel);
        void DrawObjects(
            const Grid& grid,
            const World& world,
            Mesh cylinderMesh,
            Mesh sphereMesh,
            Material material,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel);
    }
}
