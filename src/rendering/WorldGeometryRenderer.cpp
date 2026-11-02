#include "rendering/WorldGeometryRenderer.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
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
        namespace
        {
            constexpr float kDegreesToRadians = PI / 180.0f;
            constexpr float kShadowHeight = 0.006f;

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

            bool IsChunkVisible(GridCell cell, GridCell viewCenter, int chunkRadius)
            {
                const int chunkX = cell.x / kChunkSize;
                const int chunkZ = cell.z / kChunkSize;
                const int centerChunkX = viewCenter.x / kChunkSize;
                const int centerChunkZ = viewCenter.z / kChunkSize;
                return std::abs(chunkX - centerChunkX) <= chunkRadius &&
                       std::abs(chunkZ - centerChunkZ) <= chunkRadius;
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

            Mesh BuildSurfaceChunkMesh(const Grid& grid, const WorldChunk& chunk)
            {
                const int surfaceLocalY = grid.GetHeight() - 1 -
                                          chunk.coordinate.y * kChunkSize;
                if (surfaceLocalY < 0 || surfaceLocalY >= chunk.height)
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
                vertices.reserve(surfaceCellCount * 12);
                texcoords.reserve(surfaceCellCount * 8);
                normals.reserve(surfaceCellCount * 12);
                colors.reserve(surfaceCellCount * 16);
                indices.reserve(surfaceCellCount * 6);
                const float cellSize = grid.GetCellSize();
                const float topY = grid.GetWorldHeight();
                const float worldLeft = -grid.GetWorldWidth() * 0.5f;
                const float worldFront = -grid.GetWorldDepth() * 0.5f;
                for (int localZ = 0; localZ < chunk.depth; ++localZ)
                {
                    for (int localX = 0; localX < chunk.width; ++localX)
                    {
                        const std::size_t blockIndex =
                            (static_cast<std::size_t>(localZ) *
                                 static_cast<std::size_t>(chunk.height) +
                             static_cast<std::size_t>(surfaceLocalY)) *
                                static_cast<std::size_t>(chunk.width) +
                            static_cast<std::size_t>(localX);
                        const Block& block = chunk.blocks[blockIndex];
                        const float left = worldLeft +
                                           static_cast<float>(block.cell.x) * cellSize;
                        const float front = worldFront +
                                            static_cast<float>(block.cell.z) * cellSize;
                        const float right = left + cellSize;
                        const float back = front + cellSize;
                        const std::array<Vector3, 4> corners{
                            Vector3{ left, topY, front },
                            Vector3{ right, topY, front },
                            Vector3{ right, topY, back },
                            Vector3{ left, topY, back }
                        };
                        const Color color = GetBlockColor(block.type);
                        const unsigned short firstVertex =
                            static_cast<unsigned short>(vertices.size() / 3);
                        for (const Vector3 corner : corners)
                        {
                            vertices.insert(vertices.end(), { corner.x, corner.y, corner.z });
                            texcoords.insert(texcoords.end(), { 0.0f, 0.0f });
                            normals.insert(normals.end(), { 0.0f, 1.0f, 0.0f });
                            colors.insert(colors.end(), { color.r, color.g, color.b, color.a });
                        }
                        indices.insert(indices.end(), {
                            static_cast<unsigned short>(firstVertex),
                            static_cast<unsigned short>(firstVertex + 2),
                            static_cast<unsigned short>(firstVertex + 1),
                            static_cast<unsigned short>(firstVertex),
                            static_cast<unsigned short>(firstVertex + 3),
                            static_cast<unsigned short>(firstVertex + 2)
                        });
                    }
                }

                Mesh mesh{};
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

            void DrawLitMesh(
                Mesh mesh,
                Material material,
                WorldPosition position,
                Vector3 scale,
                Color color,
                float rotationAroundZ = 0.0f)
            {
                const Color previousDiffuseColor =
                    material.maps[MATERIAL_MAP_DIFFUSE].color;
                material.maps[MATERIAL_MAP_DIFFUSE].color = color;
                Matrix transform = MatrixScale(scale.x, scale.y, scale.z);
                if (rotationAroundZ != 0.0f)
                {
                    transform = MatrixMultiply(
                        transform,
                        MatrixRotate({ 0.0f, 0.0f, 1.0f }, rotationAroundZ * kDegreesToRadians));
                }
                transform = MatrixMultiply(
                    transform,
                    MatrixTranslate(position.x, position.y, position.z));
                DrawMesh(mesh, material, transform);
                material.maps[MATERIAL_MAP_DIFFUSE].color = previousDiffuseColor;
            }

            void DrawItem(
                const Grid& grid,
                const WorldObject& object,
                Mesh cylinderMesh,
                Material material)
            {
                if (!object.item.has_value())
                {
                    return;
                }

                const WorldPosition center = object.position;
                const float cellSize = grid.GetCellSize();
                if (object.item->GetType() != ItemType::Logs)
                {
                    DrawCube(
                        { center.x, center.y + cellSize * 0.16f, center.z },
                        cellSize * 0.3f,
                        cellSize * 0.3f,
                        cellSize * 0.3f,
                        { 190, 170, 125, 255 });
                    return;
                }

                const int quantity = object.item->GetQuantity();
                const int bottomRowCount = static_cast<int>(
                    (std::sqrt(8.0 * static_cast<double>(quantity) + 1.0) - 1.0) * 0.5);
                const float logRadius = cellSize * 0.09f;
                const float logLength = cellSize * 0.62f;
                const float rowSpacing = logRadius * 1.8f;
                int remainingLogs = quantity;
                int row = 0;

                while (remainingLogs > 0)
                {
                    const int desiredRowCount = row < bottomRowCount
                        ? bottomRowCount - row
                        : remainingLogs;
                    const int rowCount = std::min(desiredRowCount, remainingLogs);
                    for (int log = 0; log < rowCount; ++log)
                    {
                        const float offset =
                            (static_cast<float>(log) - static_cast<float>(rowCount - 1) * 0.5f) *
                            rowSpacing;
                        DrawLitMesh(
                            cylinderMesh,
                            material,
                            {
                                center.x + logLength * 0.5f,
                                center.y + logRadius + static_cast<float>(row) * rowSpacing,
                                center.z + offset
                            },
                            { logRadius, logLength, logRadius },
                            { 145, 95, 55, 255 },
                            90.0f);
                    }
                    remainingLogs -= rowCount;
                    ++row;
                }
            }

            void DrawWorldObject(
                const Grid& grid,
                const WorldObject& object,
                Mesh cylinderMesh,
                Mesh sphereMesh,
                Material material)
            {
                const WorldPosition cellCenter = object.position;
                const float cellSize = grid.GetCellSize();

                if (object.type == WorldObjectType::Worker)
                {
                    DrawLitMesh(
                        cylinderMesh,
                        material,
                        cellCenter,
                        { cellSize * 0.24f, cellSize * 0.76f, cellSize * 0.24f },
                        { 75, 135, 220, 255 });
                    DrawLitMesh(
                        sphereMesh,
                        material,
                        { cellCenter.x, cellCenter.y + cellSize * 0.93f, cellCenter.z },
                        { cellSize * 0.20f, cellSize * 0.20f, cellSize * 0.20f },
                        { 225, 190, 150, 255 });
                    return;
                }

                if (object.type == WorldObjectType::Tree)
                {
                    DrawLitMesh(
                        cylinderMesh,
                        material,
                        cellCenter,
                        { cellSize * 0.15f, cellSize * 0.86f, cellSize * 0.15f },
                        { 115, 75, 45, 255 });
                    DrawLitMesh(
                        sphereMesh,
                        material,
                        { cellCenter.x, cellCenter.y + cellSize * 1.2f, cellCenter.z },
                        { cellSize * 0.48f, cellSize * 0.48f, cellSize * 0.48f },
                        { 65, 145, 75, 255 });
                    return;
                }

                DrawItem(grid, object, cylinderMesh, material);
            }

            void DrawObjectShadow(const Grid& grid, const WorldObject& object)
            {
                const float cellSize = grid.GetCellSize();
                const WorldObjectSize size = GetWorldObjectSize(object.type, cellSize);
                const float shadowScale = 1.0f + size.height * 0.18f;
                const float shadowOffset = size.height * 0.12f;
                const float halfWidth = cellSize * shadowScale * 0.5f;
                const float halfDepth = cellSize * shadowScale * 0.36f;
                const float centerX = object.position.x + shadowOffset * 0.6f;
                const float centerY = object.position.y + kShadowHeight;
                const float centerZ = object.position.z - shadowOffset * 0.45f;
                const unsigned char opacity = object.type == WorldObjectType::Tree ? 150 : 110;

                rlBegin(RL_QUADS);
                    rlColor4ub(255, 255, 255, opacity);
                    rlNormal3f(0.0f, 1.0f, 0.0f);
                    rlTexCoord2f(0.0f, 0.0f);
                    rlVertex3f(centerX - halfWidth, centerY, centerZ - halfDepth);
                    rlTexCoord2f(0.0f, 1.0f);
                    rlVertex3f(centerX - halfWidth, centerY, centerZ + halfDepth);
                    rlTexCoord2f(1.0f, 1.0f);
                    rlVertex3f(centerX + halfWidth, centerY, centerZ + halfDepth);
                    rlTexCoord2f(1.0f, 0.0f);
                    rlVertex3f(centerX + halfWidth, centerY, centerZ - halfDepth);
                rlEnd();
            }
        }

        void DrawBlocks(
            const Grid& grid,
            const World& world,
            GridCell viewCenter,
            int chunkRadius,
            Material material,
            ChunkMeshCache& meshCache)
        {
            const int centerChunkX = viewCenter.x / kChunkSize;
            const int centerChunkZ = viewCenter.z / kChunkSize;
            for (const auto& [coordinate, chunk] : world.GetChunks())
            {
                if (std::abs(coordinate.x - centerChunkX) > chunkRadius ||
                    std::abs(coordinate.z - centerChunkZ) > chunkRadius)
                {
                    continue;
                }

                auto cachedMesh = meshCache.find(coordinate);
                if (cachedMesh == meshCache.end() ||
                    cachedMesh->second.terrainRevision != chunk.terrainRevision)
                {
                    if (cachedMesh != meshCache.end() &&
                        (cachedMesh->second.mesh.vaoId != 0 ||
                         cachedMesh->second.mesh.vboId != nullptr))
                    {
                        UnloadMesh(cachedMesh->second.mesh);
                    }
                    ChunkMeshCacheEntry entry{
                        BuildSurfaceChunkMesh(grid, chunk),
                        chunk.terrainRevision
                    };
                    if (cachedMesh == meshCache.end())
                    {
                        cachedMesh = meshCache.emplace(coordinate, entry).first;
                    }
                    else
                    {
                        cachedMesh->second = entry;
                    }
                }

                if (cachedMesh->second.mesh.vertexCount > 0)
                {
                    const Color previousDiffuseColor =
                        material.maps[MATERIAL_MAP_DIFFUSE].color;
                    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                    DrawMesh(cachedMesh->second.mesh, material, MatrixIdentity());
                    material.maps[MATERIAL_MAP_DIFFUSE].color = previousDiffuseColor;
                }
            }
        }

        void DrawObjectShadows(
            const Grid& grid,
            const World& world,
            GridCell viewCenter,
            int chunkRadius)
        {
            for (const WorldObject& object : world.GetObjects())
            {
                if (IsChunkVisible(object.cell, viewCenter, chunkRadius))
                {
                    DrawObjectShadow(grid, object);
                }
            }
        }

        void DrawObjects(
            const Grid& grid,
            const World& world,
            Mesh cylinderMesh,
            Mesh sphereMesh,
            Material material,
            GridCell viewCenter,
            int chunkRadius)
        {
            for (const WorldObject& object : world.GetObjects())
            {
                if (IsChunkVisible(object.cell, viewCenter, chunkRadius))
                {
                    DrawWorldObject(grid, object, cylinderMesh, sphereMesh, material);
                }
            }
        }
    }
}
