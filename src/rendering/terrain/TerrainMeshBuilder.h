#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"
#include "rendering/WorldGeometryRenderer.h"
#include "rendering/terrain/BlockTextureAtlas.h"

#include <vector>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshBuilder
        {
            Mesh BuildLevelChunkMesh(
                const Grid& grid,
                const World& world,
                const WorldChunk& chunk,
                int viewLevel,
                const BlockTextureAtlas* atlas);
        }
    }
}
