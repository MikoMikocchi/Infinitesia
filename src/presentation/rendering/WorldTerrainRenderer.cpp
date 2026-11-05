#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/terrain/TerrainMeshBuilder.h"
#include "presentation/rendering/terrain/TerrainOcclusion.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Infinitesia {
namespace WorldGeometryRenderer {
namespace {
// Chunks wholly below the cut never change with the view level, so they share
// one mesh; only the chunk holding the level is cut (even at its top block,
// whose top face must show although the chunk above is hidden).
ChunkMeshKey GetMeshKey(const WorldChunk& chunk, int viewLevel, ViewMode mode) {
  const int firstLevel = chunk.coordinate.y * kChunkSize;
  const bool isCut =
      mode == ViewMode::CutAtLevel && viewLevel < firstLevel + chunk.height;
  return {chunk.coordinate, isCut ? viewLevel : TerrainMeshBuilder::kNoCut};
}

bool IsChunkShown(const WorldChunk& chunk, int viewLevel, ViewMode mode) {
  if (IsLevelShown(mode, chunk.coordinate.y * kChunkSize, viewLevel)) {
    return true;
  }
  // A chunk of water above the viewed level stays visible: the sea is not cut
  // away with the ground.
  return mode == ViewMode::CutAtLevel && chunk.hasFluid;
}

// Chunk meshes built in one frame; more would stall the frame, so the rest
// follow on the next ones and a chunk without a mesh yet is left out for a
// moment.
constexpr int kMeshBuildsPerFrame = 6;
// Meshes of chunks this far past the drawn radius stay cached, so that moving
// the view back and forth does not unload and rebuild the same chunks.
constexpr int kRetainedChunkMargin = 4;

// The six planes of the view volume, from the projection and view the draw runs
// under.
struct Frustum {
  float planes[6][4];

  static Frustum Current() {
    const Matrix m =
        MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
    const float row[4][4] = {{m.m0, m.m4, m.m8, m.m12},
                             {m.m1, m.m5, m.m9, m.m13},
                             {m.m2, m.m6, m.m10, m.m14},
                             {m.m3, m.m7, m.m11, m.m15}};
    Frustum frustum{};
    for (int axis = 0; axis < 3; ++axis) {
      for (int i = 0; i < 4; ++i) {
        frustum.planes[axis * 2][i] = row[3][i] + row[axis][i];
        frustum.planes[axis * 2 + 1][i] = row[3][i] - row[axis][i];
      }
    }
    return frustum;
  }

  bool Intersects(const Vector3& min, const Vector3& max) const {
    for (const auto& plane : planes) {
      const float x = plane[0] >= 0.0f ? max.x : min.x;
      const float y = plane[1] >= 0.0f ? max.y : min.y;
      const float z = plane[2] >= 0.0f ? max.z : min.z;
      if (plane[0] * x + plane[1] * y + plane[2] * z + plane[3] < 0.0f) {
        return false;
      }
    }
    return true;
  }
};

bool IsMeshLoaded(const Mesh& mesh) {
  return mesh.vaoId != 0 || mesh.vboId != nullptr;
}

ChunkMeshCacheEntry& GetOrBuildChunkMesh(const Grid& grid,
                                         const VoxelWorld& world,
                                         const WorldChunk& chunk,
                                         const ChunkMeshKey& key,
                                         const BlockTextureAtlas& atlas,
                                         ChunkMeshCache& meshCache) {
  auto cachedMesh = meshCache.find(key);
  if (cachedMesh != meshCache.end() &&
      cachedMesh->second.terrainRevision == chunk.terrainRevision) {
    return cachedMesh->second;
  }

  if (cachedMesh != meshCache.end() && IsMeshLoaded(cachedMesh->second.mesh)) {
    UnloadMesh(cachedMesh->second.mesh);
  }

  ChunkMeshCacheEntry entry{TerrainMeshBuilder::BuildChunkMesh(
                                grid, world, chunk, key.second, &atlas),
                            chunk.terrainRevision};
  if (cachedMesh == meshCache.end()) {
    return meshCache.emplace(key, entry).first->second;
  }
  cachedMesh->second = entry;
  return cachedMesh->second;
}
}  // namespace

namespace {
void DrawChunkMesh(const Mesh& mesh, Material material) {
  material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  DrawMesh(mesh, material, MatrixIdentity());
}
}  // namespace

void DrawBlocks(const Grid& grid, const VoxelWorld& world, GridCell viewCenter,
                int chunkRadius, int viewLevel, ViewMode mode,
                Material material, const BlockTextureAtlas& atlas,
                ChunkMeshCache& meshCache) {
  const int centerChunkX = viewCenter.x / kChunkSize;
  const int centerChunkZ = viewCenter.z / kChunkSize;
  const auto isInRadius = [&](const ChunkCoordinate& coordinate) {
    return std::abs(coordinate.x - centerChunkX) <= chunkRadius &&
           std::abs(coordinate.z - centerChunkZ) <= chunkRadius;
  };

  static TerrainOcclusion occlusion;
  const int topShownLevel =
      mode == ViewMode::CutAtLevel ? viewLevel : grid.GetHeight() - 1;
  const Frustum frustum = Frustum::Current();
  const float chunkWorldSize =
      static_cast<float>(kChunkSize) * grid.GetCellSize();
  const auto isVisible = [&](const WorldChunk& chunk) {
    const Vector3 min{
        -grid.GetWorldWidth() * 0.5f +
            static_cast<float>(chunk.coordinate.x) * chunkWorldSize,
        static_cast<float>(chunk.coordinate.y) * chunkWorldSize,
        -grid.GetWorldDepth() * 0.5f +
            static_cast<float>(chunk.coordinate.z) * chunkWorldSize};
    const Vector3 max{
        min.x + chunkWorldSize,
        min.y + static_cast<float>(chunk.height) * grid.GetCellSize(),
        min.z + chunkWorldSize};
    return frustum.Intersects(min, max) &&
           !occlusion.IsHidden(world, chunk, topShownLevel);
  };

  int buildsLeft = kMeshBuildsPerFrame;
  std::set<ChunkMeshKey> wantedMeshes;
  for (const auto& [coordinate, chunk] : world.GetChunks()) {
    if (!isInRadius(coordinate) || !IsChunkShown(chunk, viewLevel, mode)) {
      continue;
    }

    const ChunkMeshKey key = GetMeshKey(chunk, viewLevel, mode);
    wantedMeshes.insert(key);
    if (!isVisible(chunk)) {
      continue;
    }
    const auto found = meshCache.find(key);
    const bool isCurrent =
        found != meshCache.end() &&
        found->second.terrainRevision == chunk.terrainRevision;
    if (!isCurrent && buildsLeft == 0) {
      // A stale mesh is drawn until its turn; a missing one is skipped.
      if (found == meshCache.end() || found->second.mesh.vertexCount <= 0) {
        continue;
      }
      DrawChunkMesh(found->second.mesh, material);
      continue;
    }
    if (!isCurrent) {
      --buildsLeft;
    }
    const ChunkMeshCacheEntry& cachedMesh =
        GetOrBuildChunkMesh(grid, world, chunk, key, atlas, meshCache);

    if (cachedMesh.mesh.vertexCount > 0) {
      DrawChunkMesh(cachedMesh.mesh, material);
    }
  }

  // The meshes of the next levels up and down are built ahead, one chunk each a
  // frame, so that paging through the levels does not stall on a whole layer of
  // chunks.
  if (mode == ViewMode::CutAtLevel) {
    for (const int adjacentLevel : {viewLevel - 1, viewLevel + 1}) {
      if (adjacentLevel < 0 || adjacentLevel >= grid.GetHeight()) {
        continue;
      }
      bool builtAhead = false;
      for (const auto& [coordinate, chunk] : world.GetChunks()) {
        if (!isInRadius(coordinate) ||
            !IsChunkShown(chunk, adjacentLevel, mode)) {
          continue;
        }

        const ChunkMeshKey key = GetMeshKey(chunk, adjacentLevel, mode);
        wantedMeshes.insert(key);
        const auto cachedMesh = meshCache.find(key);
        if (!builtAhead && buildsLeft > 0 &&
            (cachedMesh == meshCache.end() ||
             cachedMesh->second.terrainRevision != chunk.terrainRevision)) {
          static_cast<void>(
              GetOrBuildChunkMesh(grid, world, chunk, key, atlas, meshCache));
          builtAhead = true;
          --buildsLeft;
        }
      }
    }
  }

  for (auto cachedMesh = meshCache.begin(); cachedMesh != meshCache.end();) {
    const ChunkCoordinate& cachedCoordinate = cachedMesh->first.first;
    const bool isRetained =
        cachedMesh->first.second == TerrainMeshBuilder::kNoCut &&
        std::abs(cachedCoordinate.x - centerChunkX) <=
            chunkRadius + kRetainedChunkMargin &&
        std::abs(cachedCoordinate.z - centerChunkZ) <=
            chunkRadius + kRetainedChunkMargin;
    if (wantedMeshes.count(cachedMesh->first) != 0 || isRetained) {
      ++cachedMesh;
      continue;
    }
    if (IsMeshLoaded(cachedMesh->second.mesh)) {
      UnloadMesh(cachedMesh->second.mesh);
    }
    cachedMesh = meshCache.erase(cachedMesh);
  }
}
}  // namespace WorldGeometryRenderer
}  // namespace Infinitesia
