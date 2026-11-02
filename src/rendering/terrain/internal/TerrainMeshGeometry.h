#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"

#include <array>
#include <vector>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshGeometry
        {
            struct MeshData
            {
                std::vector<float> vertices;
                std::vector<float> texcoords;
                std::vector<float> normals;
                std::vector<unsigned char> colors;
                std::vector<unsigned short> indices;

                void AppendFace(const std::array<Vector3, 4>& corners, Vector3 normal, Color color);
                void AppendExposedSideFaces(
                    const Grid& grid,
                    const World& world,
                    GridCell cell,
                    int level,
                    float left,
                    float right,
                    float front,
                    float back,
                    float bottomY,
                    float topY,
                    Color color);
            };

            Color GetBlockColor(BlockType type);
        }
    }
}
