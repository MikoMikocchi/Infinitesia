#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/terrain/TerrainMeshBuilder.h"

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace
        {
            // Chunks wholly below the cut never change with the view level, so they share one
            // mesh; only the chunk holding the level is cut (even at its top block, whose top
            // face must show although the chunk above is hidden).
            ChunkMeshKey GetMeshKey(const WorldChunk& chunk, int viewLevel, ViewMode mode)
            {
                const int firstLevel = chunk.coordinate.y * kChunkSize;
                const bool isCut = mode == ViewMode::CutAtLevel &&
                                   viewLevel < firstLevel + chunk.height;
                return { chunk.coordinate, isCut ? viewLevel : TerrainMeshBuilder::kNoCut };
            }

            bool IsChunkShown(const ChunkCoordinate& coordinate, int viewLevel, ViewMode mode)
            {
                return IsLevelShown(mode, coordinate.y * kChunkSize, viewLevel);
            }

            bool IsMeshLoaded(const Mesh& mesh)
            {
                return mesh.vaoId != 0 || mesh.vboId != nullptr;
            }

            ChunkMeshCacheEntry& GetOrBuildChunkMesh(
                const Grid& grid,
                const VoxelWorld& world,
                const WorldChunk& chunk,
                const ChunkMeshKey& key,
                const BlockTextureAtlas& atlas,
                ChunkMeshCache& meshCache)
            {
                auto cachedMesh = meshCache.find(key);
                if (cachedMesh != meshCache.end() &&
                    cachedMesh->second.terrainRevision == chunk.terrainRevision)
                {
                    return cachedMesh->second;
                }

                if (cachedMesh != meshCache.end() && IsMeshLoaded(cachedMesh->second.mesh))
                {
                    UnloadMesh(cachedMesh->second.mesh);
                }

                ChunkMeshCacheEntry entry{
                    TerrainMeshBuilder::BuildChunkMesh(grid, world, chunk, key.second, &atlas),
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
            const VoxelWorld& world,
            GridCell viewCenter,
            int chunkRadius,
            int viewLevel,
            ViewMode mode,
            Material material,
            const BlockTextureAtlas& atlas,
            ChunkMeshCache& meshCache)
        {
            const int centerChunkX = viewCenter.x / kChunkSize;
            const int centerChunkZ = viewCenter.z / kChunkSize;
            const auto isInRadius = [&](const ChunkCoordinate& coordinate)
            {
                return std::abs(coordinate.x - centerChunkX) <= chunkRadius &&
                       std::abs(coordinate.z - centerChunkZ) <= chunkRadius;
            };

            std::set<ChunkMeshKey> wantedMeshes;
            for (const auto& [coordinate, chunk] : world.GetChunks())
            {
                if (!isInRadius(coordinate) || !IsChunkShown(coordinate, viewLevel, mode))
                {
                    continue;
                }

                const ChunkMeshKey key = GetMeshKey(chunk, viewLevel, mode);
                wantedMeshes.insert(key);
                const ChunkMeshCacheEntry& cachedMesh =
                    GetOrBuildChunkMesh(grid, world, chunk, key, atlas, meshCache);

                if (cachedMesh.mesh.vertexCount > 0)
                {
                    const Color previousDiffuseColor =
                        material.maps[MATERIAL_MAP_DIFFUSE].color;
                    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                    DrawMesh(cachedMesh.mesh, material, MatrixIdentity());
                    material.maps[MATERIAL_MAP_DIFFUSE].color = previousDiffuseColor;
                }
            }

            // The meshes of the next levels up and down are built ahead, one chunk each a
            // frame, so that paging through the levels does not stall on a whole layer of chunks.
            if (mode == ViewMode::CutAtLevel)
            {
                for (const int adjacentLevel : { viewLevel - 1, viewLevel + 1 })
                {
                    if (adjacentLevel < 0 || adjacentLevel >= grid.GetHeight())
                    {
                        continue;
                    }
                    bool builtAhead = false;
                    for (const auto& [coordinate, chunk] : world.GetChunks())
                    {
                        if (!isInRadius(coordinate) ||
                            !IsChunkShown(coordinate, adjacentLevel, mode))
                        {
                            continue;
                        }

                        const ChunkMeshKey key = GetMeshKey(chunk, adjacentLevel, mode);
                        wantedMeshes.insert(key);
                        const auto cachedMesh = meshCache.find(key);
                        if (!builtAhead &&
                            (cachedMesh == meshCache.end() ||
                             cachedMesh->second.terrainRevision != chunk.terrainRevision))
                        {
                            static_cast<void>(GetOrBuildChunkMesh(
                                grid, world, chunk, key, atlas, meshCache));
                            builtAhead = true;
                        }
                    }
                }
            }

            for (auto cachedMesh = meshCache.begin(); cachedMesh != meshCache.end();)
            {
                if (wantedMeshes.count(cachedMesh->first) != 0)
                {
                    ++cachedMesh;
                    continue;
                }
                if (IsMeshLoaded(cachedMesh->second.mesh))
                {
                    UnloadMesh(cachedMesh->second.mesh);
                }
                cachedMesh = meshCache.erase(cachedMesh);
            }
        }
    }
}
