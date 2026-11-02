#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        void DrawBlocks(const Grid& grid, const World& world);
        void DrawObjectShadows(const Grid& grid, const World& world);
        void DrawObjects(
            const Grid& grid,
            const World& world,
            Mesh cylinderMesh,
            Mesh sphereMesh,
            Material material);
    }
}
