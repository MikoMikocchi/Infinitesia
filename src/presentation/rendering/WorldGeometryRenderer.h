#pragma once

#include <cstdint>
#include <map>
#include <utility>

#include "gameplay/entities/EntityStore.h"
#include "presentation/rendering/ViewMode.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"
#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
struct ChunkMeshCacheEntry
{
	Mesh mesh{};
	std::uint64_t terrainRevision = 0;
};

struct ChunkMeshKey
{
	ChunkCoordinate coordinate;
	int cut = 0;
	// Far chunks skip ambient occlusion: the mesh is cheaper and the difference
	// is not visible at that distance.
	bool lowDetail = false;

	bool operator<(const ChunkMeshKey &other) const
	{
		if (coordinate < other.coordinate)
		{
			return true;
		}
		if (other.coordinate < coordinate)
		{
			return false;
		}
		if (cut != other.cut)
		{
			return cut < other.cut;
		}
		return lowDetail < other.lowDetail;
	}
	bool operator==(const ChunkMeshKey &) const = default;
};
using ChunkMeshCache = std::map<ChunkMeshKey, ChunkMeshCacheEntry>;

// The tiles of the block faces, worked out once (see TerrainMeshBuilder).
namespace TerrainMeshBuilder
{
struct FaceLookTable;
}

void DrawBlocks(const Grid &grid, const VoxelWorld &world, GridCell viewCenter, int chunkRadius, int viewLevel,
                ViewMode mode, Material material, const TerrainMeshBuilder::FaceLookTable &faceLooks,
                ChunkMeshCache &meshCache);
void DrawObjectShadows(const Grid &grid, const EntityStore &world, GridCell viewCenter, int chunkRadius, int viewLevel,
                       ViewMode mode);
void DrawObjects(const Grid &grid, const EntityStore &world, Mesh cylinderMesh, Mesh sphereMesh, Material material,
                 GridCell viewCenter, int chunkRadius, int viewLevel, ViewMode mode);
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
