#include "rendering/terrain/TerrainMeshBuilder.h"

#include "raymath.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace TerrainMeshBuilder
        {
            namespace
            {
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
                    }

                    throw std::invalid_argument("Block has an unsupported type.");
                }

                void AppendFace(
                    const std::array<Vector3, 4>& corners,
                    Vector3 normal,
                    Color color,
                    std::vector<float>& vertices,
                    std::vector<float>& texcoords,
                    std::vector<float>& normals,
                    std::vector<unsigned char>& colors,
                    std::vector<unsigned short>& indices)
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
                    Color color,
                    std::vector<float>& vertices,
                    std::vector<float>& texcoords,
                    std::vector<float>& normals,
                    std::vector<unsigned char>& colors,
                    std::vector<unsigned short>& indices)
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

                        AppendFace(
                            sideCorners[side],
                            sideNormals[side],
                            color,
                            vertices,
                            texcoords,
                            normals,
                            colors,
                            indices);
                    }
                }

                template<typename T>
                T* CopyMeshData(const std::vector<T>& data)
                {
                    if (data.empty())
                    {
                        return nullptr;
                    }

                    if (data.size() > std::numeric_limits<unsigned int>::max() / sizeof(T))
                    {
                        throw std::length_error("Chunk mesh data exceeds the raylib allocation limit.");
                    }

                    const unsigned int allocationSize =
                        static_cast<unsigned int>(data.size() * sizeof(T));
                    T* copy = static_cast<T*>(MemAlloc(allocationSize));
                    if (copy == nullptr)
                    {
                        throw std::bad_alloc();
                    }
                    std::memcpy(copy, data.data(), data.size() * sizeof(T));
                    return copy;
                }
            }

            Mesh BuildLevelChunkMesh(
                const Grid& grid,
                const World& world,
                const WorldChunk& chunk,
                int viewLevel)
            {
                const int localY = viewLevel - chunk.coordinate.y * kChunkSize;
                if (localY < 0 || localY >= chunk.height)
                {
                    return {};
                }

                std::vector<float> vertices;
                std::vector<float> texcoords;
                std::vector<float> normals;
                std::vector<unsigned char> colors;
                std::vector<unsigned short> indices;

                const std::size_t surfaceCellCount =
                    static_cast<std::size_t>(chunk.width) *
                    static_cast<std::size_t>(chunk.depth);
                vertices.reserve(surfaceCellCount * 60);
                texcoords.reserve(surfaceCellCount * 40);
                normals.reserve(surfaceCellCount * 60);
                colors.reserve(surfaceCellCount * 80);
                indices.reserve(surfaceCellCount * 30);
                const float cellSize = grid.GetCellSize();
                const float blockTopY = static_cast<float>(viewLevel + 1) * cellSize;
                const float floorY = static_cast<float>(viewLevel) * cellSize;
                const float worldLeft = -grid.GetWorldWidth() * 0.5f;
                const float worldFront = -grid.GetWorldDepth() * 0.5f;
                for (int localZ = 0; localZ < chunk.depth; ++localZ)
                {
                    for (int localX = 0; localX < chunk.width; ++localX)
                    {
                        const std::size_t blockIndex =
                            (static_cast<std::size_t>(localZ) *
                                 static_cast<std::size_t>(chunk.height) +
                             static_cast<std::size_t>(localY)) *
                                static_cast<std::size_t>(chunk.width) +
                            static_cast<std::size_t>(localX);
                        const Block& block = chunk.blocks[blockIndex];
                        const bool isAir = block.type == BlockType::Air;
                        const GridCell lightingCell = isAir
                            ? GridCell{ block.cell.x, viewLevel - 1, block.cell.z }
                            : block.cell;
                        const Block* floorBlock = isAir
                            ? world.GetBlock(lightingCell)
                            : nullptr;
                        if (isAir &&
                            (floorBlock == nullptr || floorBlock->type == BlockType::Air))
                        {
                            continue;
                        }

                        const float left = worldLeft +
                                           static_cast<float>(block.cell.x) * cellSize;
                        const float front = worldFront +
                                            static_cast<float>(block.cell.z) * cellSize;
                        const float right = left + cellSize;
                        const float back = front + cellSize;
                        const float topY = isAir ? floorY : blockTopY;
                        const BlockType visibleBlockType = isAir
                            ? floorBlock->type
                            : block.type;
                        Color color = GetBlockColor(visibleBlockType);
                        if (!world.HasSkyAccess(lightingCell))
                        {
                            color = BLACK;
                        }
                        AppendFace(
                            {
                                Vector3{ left, topY, front },
                                Vector3{ right, topY, front },
                                Vector3{ right, topY, back },
                                Vector3{ left, topY, back }
                            },
                            { 0.0f, 1.0f, 0.0f },
                            color,
                            vertices,
                            texcoords,
                            normals,
                            colors,
                            indices);

                        if (!isAir)
                        {
                            AppendExposedSideFaces(
                                grid,
                                world,
                                block.cell,
                                viewLevel,
                                left,
                                right,
                                front,
                                back,
                                floorY,
                                blockTopY,
                                color,
                                vertices,
                                texcoords,
                                normals,
                                colors,
                                indices);
                        }

                        if (viewLevel + 1 < grid.GetHeight())
                        {
                            const GridCell upperCell{
                                block.cell.x,
                                viewLevel + 1,
                                block.cell.z
                            };
                            const Block* upperBlock = world.GetBlock(upperCell);
                            if (upperBlock != nullptr && upperBlock->type != BlockType::Air)
                            {
                                Color upperColor = GetBlockColor(upperBlock->type);
                                if (!world.HasSkyAccess(upperCell))
                                {
                                    upperColor = BLACK;
                                }

                                AppendExposedSideFaces(
                                    grid,
                                    world,
                                    upperCell,
                                    viewLevel + 1,
                                    left,
                                    right,
                                    front,
                                    back,
                                    blockTopY,
                                    blockTopY + cellSize,
                                    upperColor,
                                    vertices,
                                    texcoords,
                                    normals,
                                    colors,
                                    indices);
                            }
                        }
                    }
                }

                Mesh mesh{};
                if (vertices.empty())
                {
                    return mesh;
                }
                mesh.vertexCount = static_cast<int>(vertices.size() / 3);
                mesh.triangleCount = static_cast<int>(indices.size() / 3);
                try
                {
                    mesh.vertices = CopyMeshData(vertices);
                    mesh.texcoords = CopyMeshData(texcoords);
                    mesh.normals = CopyMeshData(normals);
                    mesh.colors = CopyMeshData(colors);
                    mesh.indices = CopyMeshData(indices);
                }
                catch (...)
                {
                    MemFree(mesh.vertices);
                    MemFree(mesh.texcoords);
                    MemFree(mesh.normals);
                    MemFree(mesh.colors);
                    MemFree(mesh.indices);
                    throw;
                }
                UploadMesh(&mesh, false);
                return mesh;
            }
        }
    }
}
