#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"
#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"

#include <vector>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshBuilder
        {
            Mesh BuildLevelChunkMesh(
                const Grid& grid,
                const VoxelWorld& world,
                const WorldChunk& chunk,
                int viewLevel,
                const BlockTextureAtlas* atlas);
        }
    }
}
