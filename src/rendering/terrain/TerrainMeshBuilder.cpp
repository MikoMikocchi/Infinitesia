#include "rendering/terrain/TerrainMeshBuilder.h"
#include "rendering/terrain/internal/TerrainMeshGeometry.h"

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

                TerrainMeshGeometry::MeshData meshData;

                const std::size_t surfaceCellCount =
                    static_cast<std::size_t>(chunk.width) *
                    static_cast<std::size_t>(chunk.depth);
                meshData.vertices.reserve(surfaceCellCount * 60);
                meshData.texcoords.reserve(surfaceCellCount * 40);
                meshData.normals.reserve(surfaceCellCount * 60);
                meshData.colors.reserve(surfaceCellCount * 80);
                meshData.indices.reserve(surfaceCellCount * 30);
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
                        Color color = TerrainMeshGeometry::GetBlockColor(visibleBlockType);
                        if (!world.HasSkyAccess(lightingCell))
                        {
                            color = BLACK;
                        }
                        meshData.AppendFace(
                            {
                                Vector3{ left, topY, front },
                                Vector3{ right, topY, front },
                                Vector3{ right, topY, back },
                                Vector3{ left, topY, back }
                            },
                            { 0.0f, 1.0f, 0.0f },
                            color);

                        if (!isAir)
                        {
                            meshData.AppendExposedSideFaces(
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
                                color);
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
                                Color upperColor =
                                    TerrainMeshGeometry::GetBlockColor(upperBlock->type);
                                if (!world.HasSkyAccess(upperCell))
                                {
                                    upperColor = BLACK;
                                }

                                meshData.AppendExposedSideFaces(
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
                                    upperColor);
                            }
                        }
                    }
                }

                Mesh mesh{};
                if (meshData.vertices.empty())
                {
                    return mesh;
                }
                mesh.vertexCount = static_cast<int>(meshData.vertices.size() / 3);
                mesh.triangleCount = static_cast<int>(meshData.indices.size() / 3);
                try
                {
                    mesh.vertices = CopyMeshData(meshData.vertices);
                    mesh.texcoords = CopyMeshData(meshData.texcoords);
                    mesh.normals = CopyMeshData(meshData.normals);
                    mesh.colors = CopyMeshData(meshData.colors);
                    mesh.indices = CopyMeshData(meshData.indices);
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
