#include "rendering/terrain/internal/TerrainMeshGeometry.h"

#include "raymath.h"

#include <stdexcept>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshGeometry
        {
            namespace
            {
                void AppendFaceIndices(
                    const std::array<Vector3, 4>& corners,
                    Vector3 normal,
                    unsigned short firstVertex,
                    std::vector<unsigned short>& indices)
                {
                    const Vector3 firstEdge = Vector3Subtract(corners[1], corners[0]);
                    const Vector3 secondEdge = Vector3Subtract(corners[2], corners[0]);
                    const bool reverseWinding =
                        Vector3DotProduct(Vector3CrossProduct(firstEdge, secondEdge), normal) < 0.0f;
                    if (reverseWinding)
                    {
                        indices.insert(indices.end(), {
                            firstVertex,
                            static_cast<unsigned short>(firstVertex + 2),
                            static_cast<unsigned short>(firstVertex + 1),
                            firstVertex,
                            static_cast<unsigned short>(firstVertex + 3),
                            static_cast<unsigned short>(firstVertex + 2)
                        });
                        return;
                    }

                    indices.insert(indices.end(), {
                        firstVertex,
                        static_cast<unsigned short>(firstVertex + 1),
                        static_cast<unsigned short>(firstVertex + 2),
                        firstVertex,
                        static_cast<unsigned short>(firstVertex + 2),
                        static_cast<unsigned short>(firstVertex + 3)
                    });
                }
            }

            Color GetBlockColor(BlockType type)
            {
                switch (type)
                {
                case BlockType::Grass:
                    return { 82, 145, 66, 255 };
                case BlockType::Dirt:
                    return { 139, 94, 60, 255 };
                case BlockType::Stone:
                    return { 132, 137, 142, 255 };
                case BlockType::Sand:
                    return { 219, 203, 140, 255 };
                case BlockType::Water:
                    return { 52, 110, 170, 255 };
                case BlockType::Air:
                    break;
                }

                throw std::invalid_argument("Block has an unsupported type.");
            }

            void MeshData::AppendFace(
                const std::array<Vector3, 4>& corners,
                Vector3 normal,
                Color color)
            {
                const unsigned short firstVertex =
                    static_cast<unsigned short>(vertices.size() / 3);
                for (const Vector3 corner : corners)
                {
                    vertices.insert(vertices.end(), { corner.x, corner.y, corner.z });
                    texcoords.insert(texcoords.end(), { 0.0f, 0.0f });
                    normals.insert(normals.end(), { normal.x, normal.y, normal.z });
                    colors.insert(colors.end(), { color.r, color.g, color.b, color.a });
                }

                AppendFaceIndices(corners, normal, firstVertex, indices);
            }

            void MeshData::AppendExposedSideFaces(
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
                Color color)
            {
                const std::array<GridCell, 4> neighbors{
                    GridCell{ cell.x - 1, level, cell.z },
                    GridCell{ cell.x + 1, level, cell.z },
                    GridCell{ cell.x, level, cell.z - 1 },
                    GridCell{ cell.x, level, cell.z + 1 }
                };
                const std::array<std::array<Vector3, 4>, 4> sideCorners{
                    std::array<Vector3, 4>{
                        Vector3{ left, bottomY, front },
                        Vector3{ left, bottomY, back },
                        Vector3{ left, topY, back },
                        Vector3{ left, topY, front }
                    },
                    std::array<Vector3, 4>{
                        Vector3{ right, bottomY, back },
                        Vector3{ right, bottomY, front },
                        Vector3{ right, topY, front },
                        Vector3{ right, topY, back }
                    },
                    std::array<Vector3, 4>{
                        Vector3{ right, bottomY, front },
                        Vector3{ left, bottomY, front },
                        Vector3{ left, topY, front },
                        Vector3{ right, topY, front }
                    },
                    std::array<Vector3, 4>{
                        Vector3{ left, bottomY, back },
                        Vector3{ right, bottomY, back },
                        Vector3{ right, topY, back },
                        Vector3{ left, topY, back }
                    }
                };
                const std::array<Vector3, 4> sideNormals{
                    Vector3{ -1.0f, 0.0f, 0.0f },
                    Vector3{ 1.0f, 0.0f, 0.0f },
                    Vector3{ 0.0f, 0.0f, -1.0f },
                    Vector3{ 0.0f, 0.0f, 1.0f }
                };

                for (std::size_t side = 0; side < neighbors.size(); ++side)
                {
                    if (grid.Contains(neighbors[side]))
                    {
                        const Block* neighbor = world.GetBlock(neighbors[side]);
                        if (neighbor == nullptr || neighbor->type != BlockType::Air)
                        {
                            continue;
                        }
                    }

                    AppendFace(sideCorners[side], sideNormals[side], color);
                }
            }
        }
    }
}
