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

            bool IsChunkVisible(
                GridCell cell,
                GridCell viewCenter,
                int chunkRadius,
                int viewLevel)
            {
                const int chunkX = cell.x / kChunkSize;
                const int chunkZ = cell.z / kChunkSize;
                const int centerChunkX = viewCenter.x / kChunkSize;
                const int centerChunkZ = viewCenter.z / kChunkSize;
                return cell.y == viewLevel &&
                       std::abs(chunkX - centerChunkX) <= chunkRadius &&
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
            int viewLevel,
            Material material,
            ChunkMeshCache& meshCache)
        {
            const int centerChunkX = viewCenter.x / kChunkSize;
            const int centerChunkZ = viewCenter.z / kChunkSize;
            for (const auto& [coordinate, chunk] : world.GetChunks())
            {
                if (std::abs(coordinate.x - centerChunkX) > chunkRadius ||
                    std::abs(coordinate.z - centerChunkZ) > chunkRadius ||
                    viewLevel < coordinate.y * kChunkSize ||
                    viewLevel >= (coordinate.y + 1) * kChunkSize)
                {
                    continue;
                }

                auto cachedMesh = meshCache.find(coordinate);
                if (cachedMesh == meshCache.end() ||
                    cachedMesh->second.terrainRevision != chunk.terrainRevision ||
                    cachedMesh->second.worldTerrainRevision != world.GetTerrainRevision() ||
                    cachedMesh->second.level != viewLevel)
                {
                    if (cachedMesh != meshCache.end() &&
                        (cachedMesh->second.mesh.vaoId != 0 ||
                         cachedMesh->second.mesh.vboId != nullptr))
                    {
                        UnloadMesh(cachedMesh->second.mesh);
                    }
                    ChunkMeshCacheEntry entry{
                        BuildLevelChunkMesh(grid, world, chunk, viewLevel),
                        chunk.terrainRevision,
                        world.GetTerrainRevision(),
                        viewLevel
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
            int chunkRadius,
            int viewLevel)
        {
            for (const WorldObject& object : world.GetObjects())
            {
                if (IsChunkVisible(object.cell, viewCenter, chunkRadius, viewLevel))
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
            int chunkRadius,
            int viewLevel)
        {
            for (const WorldObject& object : world.GetObjects())
            {
                if (IsChunkVisible(object.cell, viewCenter, chunkRadius, viewLevel))
                {
                    DrawWorldObject(grid, object, cylinderMesh, sphereMesh, material);
                }
            }
        }
    }
}
