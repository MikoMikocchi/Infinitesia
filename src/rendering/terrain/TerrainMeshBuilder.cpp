#include "rendering/terrain/TerrainMeshBuilder.h"
#include "rendering/terrain/internal/TerrainMeshGeometry.h"

#include <algorithm>
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
                // How far below the cutting plane an empty cell looks for ground, and how
                // fast that ground darkens with depth.
                constexpr int kMaxVisibleDepth = 16;
                constexpr float kFloorShadePerLevel = 0.04f;
                constexpr float kDeepestFloorShade = 0.4f;

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
                int viewLevel,
                const BlockTextureAtlas* atlas)
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

                        // An empty cell shows the first solid block below it, drawn on the
                        // cutting plane and darkened the deeper it lies.
                        const Block* floorBlock = nullptr;
                        int floorDepth = 0;
                        if (isAir)
                        {
                            for (int depth = 1; depth <= kMaxVisibleDepth; ++depth)
                            {
                                const Block* below = world.GetBlock(
                                    GridCell{ block.cell.x, viewLevel - depth, block.cell.z });
                                if (below == nullptr)
                                {
                                    break;
                                }
                                if (below->type != BlockType::Air)
                                {
                                    floorBlock = below;
                                    floorDepth = depth;
                                    break;
                                }
                            }
                            if (floorBlock == nullptr)
                            {
                                continue;
                            }
                        }
                        const GridCell lightingCell = isAir ? floorBlock->cell : block.cell;

                        const float left = worldLeft +
                                           static_cast<float>(block.cell.x) * cellSize;
                        const float front = worldFront +
                                            static_cast<float>(block.cell.z) * cellSize;
                        const float right = left + cellSize;
                        const float back = front + cellSize;
                        const float topY = isAir ? floorY : blockTopY;
                        const Block& visibleBlock = isAir ? *floorBlock : block;
                        TerrainMeshGeometry::FaceLook topLook = TerrainMeshGeometry::GetFaceLook(
                            visibleBlock, world.GetCatalog(), atlas, TerrainMeshGeometry::FaceKind::Top);
                        TerrainMeshGeometry::FaceLook sideLook = TerrainMeshGeometry::GetFaceLook(
                            visibleBlock, world.GetCatalog(), atlas, TerrainMeshGeometry::FaceKind::Side);
                        if (!world.HasSkyAccess(lightingCell))
                        {
                            topLook.tint = BLACK;
                            sideLook.tint = BLACK;
                        }
                        else if (isAir && floorDepth > 1)
                        {
                            const float shade = std::max(
                                kDeepestFloorShade, 1.0f - kFloorShadePerLevel * static_cast<float>(floorDepth - 1));
                            for (TerrainMeshGeometry::FaceLook* look : { &topLook, &sideLook })
                            {
                                look->tint = {
                                    static_cast<unsigned char>(look->tint.r * shade),
                                    static_cast<unsigned char>(look->tint.g * shade),
                                    static_cast<unsigned char>(look->tint.b * shade),
                                    look->tint.a
                                };
                            }
                        }
                        meshData.AppendFace(
                            {
                                Vector3{ left, topY, front },
                                Vector3{ right, topY, front },
                                Vector3{ right, topY, back },
                                Vector3{ left, topY, back }
                            },
                            { 0.0f, 1.0f, 0.0f },
                            topLook);

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
                                sideLook);
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
                                TerrainMeshGeometry::FaceLook upperLook = TerrainMeshGeometry::GetFaceLook(
                                    *upperBlock, world.GetCatalog(), atlas, TerrainMeshGeometry::FaceKind::Side);
                                if (!world.HasSkyAccess(upperCell))
                                {
                                    upperLook.tint = BLACK;
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
                                    upperLook);
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
