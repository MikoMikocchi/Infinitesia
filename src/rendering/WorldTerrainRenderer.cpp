#include "rendering/WorldGeometryRenderer.h"
#include "rendering/terrain/TerrainMeshBuilder.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        void DrawBlocks(
            const Grid& grid,
            const World& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel,
            Material material,
            ChunkMeshCache& meshCache)
        {
            const int centerChunkX = viewCenter.x / kChunkSize;
            const int centerChunkZ = viewCenter.z / kChunkSize;
            for (const auto& [coordinate, chunk] : world.GetChunks())
            {
                if (std::abs(coordinate.x - centerChunkX) > chunkRadius ||
                    std::abs(coordinate.z - centerChunkZ) > chunkRadius ||
                    viewLevel < coordinate.y * kChunkSize ||
                    viewLevel >= (coordinate.y + 1) * kChunkSize)
                {
                    continue;
                }

                auto cachedMesh = meshCache.find(coordinate);
                if (cachedMesh == meshCache.end() ||
                    cachedMesh->second.terrainRevision != chunk.terrainRevision ||
                    cachedMesh->second.worldTerrainRevision != world.GetTerrainRevision() ||
                    cachedMesh->second.level != viewLevel)
                {
                    if (cachedMesh != meshCache.end() &&
                        (cachedMesh->second.mesh.vaoId != 0 ||
                         cachedMesh->second.mesh.vboId != nullptr))
                    {
                        UnloadMesh(cachedMesh->second.mesh);
                    }
                    ChunkMeshCacheEntry entry{
                        TerrainMeshBuilder::BuildLevelChunkMesh(grid, world, chunk, viewLevel),
                        chunk.terrainRevision,
                        world.GetTerrainRevision(),
                        viewLevel
                    };
                    if (cachedMesh == meshCache.end())
                    {
                        cachedMesh = meshCache.emplace(coordinate, entry).first;
                    }
                    else
                    {
                        cachedMesh->second = entry;
                    }
                }

                if (cachedMesh->second.mesh.vertexCount > 0)
                {
                    const Color previousDiffuseColor =
                        material.maps[MATERIAL_MAP_DIFFUSE].color;
                    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                    DrawMesh(cachedMesh->second.mesh, material, MatrixIdentity());
                    material.maps[MATERIAL_MAP_DIFFUSE].color = previousDiffuseColor;
                }
            }
        }
    }
}
