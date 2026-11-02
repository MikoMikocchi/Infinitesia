#include "rendering/WorldGeometryRenderer.h"

#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    namespace WorldGeometryRenderer
    {
        namespace
        {
            constexpr float kDegreesToRadians = PI / 180.0f;
            constexpr float kShadowHeight = 0.006f;

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
