#pragma once

#include <vector>

#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"
#include "presentation/rendering/terrain/internal/TerrainMeshGeometry.h"
#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace TerrainMeshBuilder
{
// Cut level meaning "no cut": the whole chunk is meshed.
constexpr int kNoCut = -1;

// The tile of a matter's top and side face, worked out once: meshing a chunk
// would otherwise build a key string and a hash entry for every block it sees.
struct FaceLookTable
{
	std::vector<TerrainMeshGeometry::FaceLook> top; // by MatterId
	std::vector<TerrainMeshGeometry::FaceLook> side;
};

void BuildFaceLookTable(const BlockRegistry &registry, const BlockTextureAtlas *atlas, FaceLookTable &table);

// The solid blocks of the chunk with only their exposed faces. With a cut
// level, blocks above that level are left out and the blocks on it get their
// top face whatever lies over them, as a cut through the ground.
Mesh BuildChunkMesh(const Grid &grid, const VoxelWorld &world, const WorldChunk &chunk, int cutLevel,
                    const FaceLookTable &faceLooks, bool lowDetail);
} // namespace TerrainMeshBuilder
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
