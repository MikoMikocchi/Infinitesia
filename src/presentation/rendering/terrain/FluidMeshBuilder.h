#pragma once

#include <cstdint>
#include <map>
#include <utility>

#include "presentation/rendering/ViewMode.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"
#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
// A water mesh is a flat, translucent top surface per column; it is kept apart
// from the opaque block meshes of the chunk.
constexpr int kNoFluidCut = -1;

struct FluidMeshCacheEntry
{
	Mesh mesh{};
	std::uint64_t fluidRevision = 0;
	// Space allocated in the mesh buffers (in vertices and triangles); a rebuilt
	// mesh that fits is copied into them instead of recreating the buffers.
	int vertexCapacity = 0;
	int triangleCapacity = 0;
};

using FluidMeshKey = std::pair<ChunkCoordinate, int>;
using FluidMeshCache = std::map<FluidMeshKey, FluidMeshCacheEntry>;

// The top quads of the fluid columns of one chunk, at a cut level or whole, on
// the CPU: the caller uploads or reuses a GPU mesh for it.
Mesh BuildFluidMeshData(const Grid &grid, const VoxelWorld &world, const FluidChunk &chunk, int cutLevel,
                        const BlockTextureAtlas *atlas);

// Draws the fluid of the loaded chunks in the view radius, rebuilding only the
// meshes whose fluid changed. Call it after the opaque terrain with alpha
// blending on and the depth mask off.
void DrawFluids(const Grid &grid, const VoxelWorld &world, GridCell viewCenter, int chunkRadius, int viewLevel,
                ViewMode mode, Material material, const BlockTextureAtlas &atlas, FluidMeshCache &meshCache);
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
