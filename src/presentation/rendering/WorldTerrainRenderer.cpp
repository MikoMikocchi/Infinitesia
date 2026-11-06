#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <set>
#include <vector>

#include "presentation/rendering/ViewFrustum.h"
#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/terrain/TerrainMeshBuilder.h"
#include "presentation/rendering/terrain/TerrainOcclusion.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace
{
// Chunks wholly below the cut never change with the view level, so they share
// one mesh; only the chunk holding the level is cut (even at its top block,
// whose top face must show although the chunk above is hidden).
ChunkMeshKey GetMeshKey(const WorldChunk &chunk, int viewLevel, ViewMode mode)
{
	const int firstLevel = chunk.coordinate.y * kChunkSize;
	const bool isCut = mode == ViewMode::CutAtLevel && viewLevel < firstLevel + chunk.height;
	return {chunk.coordinate, isCut ? viewLevel : TerrainMeshBuilder::kNoCut};
}

bool IsChunkShown(const ChunkCoordinate &coordinate, int viewLevel, ViewMode mode)
{
	return IsLevelShown(mode, coordinate.y * kChunkSize, viewLevel);
}

// Chunk meshes may be built for this long in one frame (and never more than the
// hard cap): a fast frame fills the view quickly, a slow one does not stall, and
// a chunk without a mesh yet is left out for a moment.
constexpr double kMeshBuildBudgetMs = 2.0;
constexpr int kMeshBuildsPerFrame = 24;
// Meshes of chunks this far past the drawn radius stay cached, so that moving
// the view back and forth does not unload and rebuild the same chunks.
constexpr int kRetainedChunkMargin = 4;

using BuildClock = std::chrono::steady_clock;

bool IsBudgetSpent(const BuildClock::time_point &deadline, int buildsLeft)
{
	return buildsLeft <= 0 || BuildClock::now() >= deadline;
}

bool IsMeshLoaded(const Mesh &mesh)
{
	return mesh.vaoId != 0 || mesh.vboId != nullptr;
}

ChunkMeshCacheEntry &GetOrBuildChunkMesh(const Grid &grid, const VoxelWorld &world, const WorldChunk &chunk,
                                         const ChunkMeshKey &key, const TerrainMeshBuilder::FaceLookTable &faceLooks,
                                         ChunkMeshCache &meshCache)
{
	auto cachedMesh = meshCache.find(key);
	if (cachedMesh != meshCache.end() && cachedMesh->second.terrainRevision == chunk.terrainRevision)
	{
		return cachedMesh->second;
	}

	if (cachedMesh != meshCache.end() && IsMeshLoaded(cachedMesh->second.mesh))
	{
		UnloadMesh(cachedMesh->second.mesh);
	}

	ChunkMeshCacheEntry entry{TerrainMeshBuilder::BuildChunkMesh(grid, world, chunk, key.second, faceLooks),
	                          chunk.terrainRevision};
	if (cachedMesh == meshCache.end())
	{
		return meshCache.emplace(key, entry).first->second;
	}
	cachedMesh->second = entry;
	return cachedMesh->second;
}
} // namespace

namespace
{
void DrawChunkMesh(const Mesh &mesh, Material material)
{
	material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
	DrawMesh(mesh, material, MatrixIdentity());
}
} // namespace

void DrawBlocks(const Grid &grid, const VoxelWorld &world, GridCell viewCenter, int chunkRadius, int viewLevel,
                ViewMode mode, Material material, const TerrainMeshBuilder::FaceLookTable &faceLooks,
                ChunkMeshCache &meshCache)
{
	const int centerChunkX = viewCenter.x / kChunkSize;
	const int centerChunkZ = viewCenter.z / kChunkSize;
	const auto isInRadius = [&](const ChunkCoordinate &coordinate) {
		return std::abs(coordinate.x - centerChunkX) <= chunkRadius &&
		       std::abs(coordinate.z - centerChunkZ) <= chunkRadius;
	};

	static TerrainOcclusion occlusion;
	const int topShownLevel = mode == ViewMode::CutAtLevel ? viewLevel : grid.GetHeight() - 1;
	const Frustum frustum = Frustum::Current();
	const float chunkWorldSize = static_cast<float>(kChunkSize) * grid.GetCellSize();
	const auto isVisible = [&](const WorldChunk &chunk) {
		const Vector3 min{-grid.GetWorldWidth() * 0.5f + static_cast<float>(chunk.coordinate.x) * chunkWorldSize,
		                  static_cast<float>(chunk.coordinate.y) * chunkWorldSize,
		                  -grid.GetWorldDepth() * 0.5f + static_cast<float>(chunk.coordinate.z) * chunkWorldSize};
		const Vector3 max{min.x + chunkWorldSize, min.y + static_cast<float>(chunk.height) * grid.GetCellSize(),
		                  min.z + chunkWorldSize};
		return frustum.Intersects(min, max) && !occlusion.IsHidden(world, chunk, topShownLevel);
	};

	const BuildClock::time_point buildDeadline =
	    BuildClock::now() + std::chrono::duration_cast<BuildClock::duration>(
	                             std::chrono::duration<double, std::milli>(kMeshBuildBudgetMs));
	int buildsLeft = kMeshBuildsPerFrame;
	std::set<ChunkMeshKey> wantedMeshes;

	// Stale meshes are built nearest first, so the ground under the view appears
	// before the far edge.
	struct PendingBuild
	{
		int distanceSquared;
		const WorldChunk *chunk;
		ChunkMeshKey key;
	};
	const auto distanceTo = [&](const ChunkCoordinate &coordinate, int level) {
		const int dx = coordinate.x - centerChunkX;
		const int dy = coordinate.y - level / kChunkSize;
		const int dz = coordinate.z - centerChunkZ;
		return dx * dx + dy * dy + dz * dz;
	};
	std::vector<PendingBuild> pending;
	pending.reserve(64);

	for (const auto &[coordinate, chunk] : world.GetChunks())
	{
		if (!isInRadius(coordinate) || !IsChunkShown(coordinate, viewLevel, mode))
		{
			continue;
		}

		const ChunkMeshKey key = GetMeshKey(chunk, viewLevel, mode);
		wantedMeshes.insert(key);
		if (!isVisible(chunk))
		{
			continue;
		}
		const auto found = meshCache.find(key);
		if (found != meshCache.end() && found->second.terrainRevision == chunk.terrainRevision)
		{
			if (found->second.mesh.vertexCount > 0)
			{
				DrawChunkMesh(found->second.mesh, material);
			}
			continue;
		}
		pending.push_back({distanceTo(coordinate, viewLevel), &chunk, key});
	}

	std::sort(pending.begin(), pending.end(), [](const PendingBuild &left, const PendingBuild &right) {
		return left.distanceSquared < right.distanceSquared;
	});
	for (const PendingBuild &build : pending)
	{
		const auto found = meshCache.find(build.key);
		if (IsBudgetSpent(buildDeadline, buildsLeft))
		{
			// A stale mesh is drawn until its turn; a missing one is skipped.
			if (found != meshCache.end() && found->second.mesh.vertexCount > 0)
			{
				DrawChunkMesh(found->second.mesh, material);
			}
			continue;
		}
		--buildsLeft;
		const ChunkMeshCacheEntry &cachedMesh =
		    GetOrBuildChunkMesh(grid, world, *build.chunk, build.key, faceLooks, meshCache);
		if (cachedMesh.mesh.vertexCount > 0)
		{
			DrawChunkMesh(cachedMesh.mesh, material);
		}
	}

	// The meshes of the next levels up and down are built ahead with what is left
	// of this frame's build budget, so that paging through the levels does not
	// stall on a whole layer of chunks.
	if (mode == ViewMode::CutAtLevel)
	{
		std::vector<PendingBuild> ahead;
		ahead.reserve(64);
		for (const int adjacentLevel : {viewLevel - 1, viewLevel + 1})
		{
			if (adjacentLevel < 0 || adjacentLevel >= grid.GetHeight())
			{
				continue;
			}
			for (const auto &[coordinate, chunk] : world.GetChunks())
			{
				if (!isInRadius(coordinate) || !IsChunkShown(coordinate, adjacentLevel, mode))
				{
					continue;
				}

				const ChunkMeshKey key = GetMeshKey(chunk, adjacentLevel, mode);
				wantedMeshes.insert(key);
				const auto cachedMesh = meshCache.find(key);
				if (cachedMesh != meshCache.end() && cachedMesh->second.terrainRevision == chunk.terrainRevision)
				{
					continue;
				}
				ahead.push_back({distanceTo(coordinate, viewLevel), &chunk, key});
			}
		}
		std::sort(ahead.begin(), ahead.end(), [](const PendingBuild &left, const PendingBuild &right) {
			return left.distanceSquared < right.distanceSquared;
		});
		for (const PendingBuild &build : ahead)
		{
			if (IsBudgetSpent(buildDeadline, buildsLeft))
			{
				break;
			}
			--buildsLeft;
			static_cast<void>(GetOrBuildChunkMesh(grid, world, *build.chunk, build.key, faceLooks, meshCache));
		}
	}

	for (auto cachedMesh = meshCache.begin(); cachedMesh != meshCache.end();)
	{
		const ChunkCoordinate &cachedCoordinate = cachedMesh->first.first;
		const int cachedCut = cachedMesh->first.second;
		const bool inMargin = std::abs(cachedCoordinate.x - centerChunkX) <= chunkRadius + kRetainedChunkMargin &&
		                      std::abs(cachedCoordinate.z - centerChunkZ) <= chunkRadius + kRetainedChunkMargin;
		// The neighbouring levels are kept so that paging up and down is instant.
		const bool isNeighbourCut = cachedCut != TerrainMeshBuilder::kNoCut &&
		                            std::abs(cachedCut - viewLevel) <= 1 &&
		                            std::abs(cachedCoordinate.x - centerChunkX) <= chunkRadius &&
		                            std::abs(cachedCoordinate.z - centerChunkZ) <= chunkRadius;
		const bool isRetained = (cachedCut == TerrainMeshBuilder::kNoCut && inMargin) || isNeighbourCut;
		if (wantedMeshes.count(cachedMesh->first) != 0 || isRetained)
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
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
