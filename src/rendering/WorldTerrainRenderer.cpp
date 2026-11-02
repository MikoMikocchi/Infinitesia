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
        namespace
        {
            ChunkMeshCacheEntry& GetOrBuildChunkMesh(
                const Grid& grid,
                const World& world,
                const WorldChunk& chunk,
                int viewLevel,
                ChunkMeshCache& meshCache)
            {
                const ChunkMeshKey key{ chunk.coordinate, viewLevel };
                auto cachedMesh = meshCache.find(key);
                if (cachedMesh != meshCache.end() &&
                    cachedMesh->second.terrainRevision == chunk.terrainRevision)
                {
                    return cachedMesh->second;
                }

                if (cachedMesh != meshCache.end() &&
                    (cachedMesh->second.mesh.vaoId != 0 ||
                     cachedMesh->second.mesh.vboId != nullptr))
                {
                    UnloadMesh(cachedMesh->second.mesh);
                }

                ChunkMeshCacheEntry entry{
                    TerrainMeshBuilder::BuildLevelChunkMesh(grid, world, chunk, viewLevel),
                    chunk.terrainRevision
                };
                if (cachedMesh == meshCache.end())
                {
                    return meshCache.emplace(key, entry).first->second;
                }
                cachedMesh->second = entry;
                return cachedMesh->second;
            }
        }

        void DrawBlocks(
            const Grid& grid,
            const World& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel,
            Material material,
            ChunkMeshCache& meshCache)
        {
            for (auto cachedMesh = meshCache.begin(); cachedMesh != meshCache.end();)
            {
                if (std::abs(cachedMesh->first.second - viewLevel) <= 1)
                {
                    ++cachedMesh;
                    continue;
                }
                if (cachedMesh->second.mesh.vaoId != 0 ||
                    cachedMesh->second.mesh.vboId != nullptr)
                {
                    UnloadMesh(cachedMesh->second.mesh);
                }
                cachedMesh = meshCache.erase(cachedMesh);
            }

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

                const ChunkMeshCacheEntry& cachedMesh =
                    GetOrBuildChunkMesh(grid, world, chunk, viewLevel, meshCache);

                if (cachedMesh.mesh.vertexCount > 0)
                {
                    const Color previousDiffuseColor =
                        material.maps[MATERIAL_MAP_DIFFUSE].color;
                    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                    DrawMesh(cachedMesh.mesh, material, MatrixIdentity());
                    material.maps[MATERIAL_MAP_DIFFUSE].color = previousDiffuseColor;
                }
            }

            for (const int adjacentLevel : { viewLevel - 1, viewLevel + 1 })
            {
                if (adjacentLevel < 0 || adjacentLevel >= grid.GetHeight())
                {
                    continue;
                }
                for (const auto& [coordinate, chunk] : world.GetChunks())
                {
                    if (std::abs(coordinate.x - centerChunkX) > chunkRadius ||
                        std::abs(coordinate.z - centerChunkZ) > chunkRadius ||
                        adjacentLevel < coordinate.y * kChunkSize ||
                        adjacentLevel >= (coordinate.y + 1) * kChunkSize)
                    {
                        continue;
                    }

                    const ChunkMeshKey key{ coordinate, adjacentLevel };
                    const auto cachedMesh = meshCache.find(key);
                    if (cachedMesh != meshCache.end() &&
                        cachedMesh->second.terrainRevision == chunk.terrainRevision)
                    {
                        continue;
                    }

                    static_cast<void>(GetOrBuildChunkMesh(
                        grid,
                        world,
                        chunk,
                        adjacentLevel,
                        meshCache));
                    break;
                }
            }
        }
    }
}
