#include "rendering/WorldRenderer.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr float kDegreesToRadians = PI / 180.0f;
        constexpr float kGridLineHeight = 0.01f;
        constexpr float kHighlightHeight = 0.005f;
        constexpr Color kGridLineColor = { 85, 95, 105, 255 };
        constexpr Color kGridBoundaryColor = { 145, 155, 165, 255 };
        constexpr Color kHoveredCellColor = { 245, 190, 55, 100 };
        constexpr Color kHoveredCellOutlineColor = { 255, 215, 90, 255 };
        constexpr Color kHoveredObjectOutlineColor = { 245, 245, 245, 255 };
        constexpr Color kSelectedObjectOutlineColor = { 70, 220, 110, 255 };
        constexpr Color kDebugPathColor = { 70, 205, 235, 230 };
        constexpr Color kDebugPathBendColor = { 255, 195, 65, 255 };

        Camera3D CreateCamera(CameraConfig config)
        {
            if (!std::isfinite(config.distance) || config.distance <= 0.0f)
            {
                throw std::invalid_argument("Camera distance must be finite and greater than zero.");
            }

            if (!std::isfinite(config.elevationDegrees) ||
                config.elevationDegrees <= 0.0f || config.elevationDegrees >= 90.0f)
            {
                throw std::invalid_argument("Camera elevation must be between zero and 90 degrees.");
            }

            if (!std::isfinite(config.fieldOfViewDegrees) ||
                config.fieldOfViewDegrees <= 0.0f || config.fieldOfViewDegrees >= 180.0f)
            {
                throw std::invalid_argument("Camera field of view must be between zero and 180 degrees.");
            }

            const float elevationRadians = config.elevationDegrees * kDegreesToRadians;
            return {
                { 0.0f, config.distance * std::sin(elevationRadians), config.distance * std::cos(elevationRadians) },
                { 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f },
                config.fieldOfViewDegrees,
                CAMERA_PERSPECTIVE
            };
        }

        void DrawGridLines(const Grid& grid)
        {
            const float halfWidth = grid.GetWorldWidth() * 0.5f;
            const float halfDepth = grid.GetWorldDepth() * 0.5f;
            const float lineY = grid.GetWorldHeight() + kGridLineHeight;

            for (std::int64_t column = 0; column <= grid.GetWidth(); ++column)
            {
                const float x = -halfWidth + static_cast<float>(column) * grid.GetCellSize();
                const Color color = (column == 0 || column == grid.GetWidth())
                    ? kGridBoundaryColor
                    : kGridLineColor;
                DrawLine3D({ x, lineY, -halfDepth }, { x, lineY, halfDepth }, color);
            }

            for (std::int64_t row = 0; row <= grid.GetDepth(); ++row)
            {
                const float z = -halfDepth + static_cast<float>(row) * grid.GetCellSize();
                const Color color = (row == 0 || row == grid.GetDepth())
                    ? kGridBoundaryColor
                    : kGridLineColor;
                DrawLine3D({ -halfWidth, lineY, z }, { halfWidth, lineY, z }, color);
            }
        }

        void DrawHoveredCell(const Grid& grid, GridCell cell)
        {
            if (!grid.Contains(cell))
            {
                return;
            }

            const WorldPosition center = grid.CellToSurface(cell);
            const float halfCell = grid.GetCellSize() * 0.5f;
            const float left = center.x - halfCell;
            const float right = center.x + halfCell;
            const float top = center.z - halfCell;
            const float bottom = center.z + halfCell;

            DrawPlane(
                { center.x, center.y + kHighlightHeight, center.z },
                { grid.GetCellSize(), grid.GetCellSize() },
                kHoveredCellColor);
            const float outlineY = center.y + kHighlightHeight;
            DrawLine3D({ left, outlineY, top }, { right, outlineY, top }, kHoveredCellOutlineColor);
            DrawLine3D({ right, outlineY, top }, { right, outlineY, bottom }, kHoveredCellOutlineColor);
            DrawLine3D({ right, outlineY, bottom }, { left, outlineY, bottom }, kHoveredCellOutlineColor);
            DrawLine3D({ left, outlineY, bottom }, { left, outlineY, top }, kHoveredCellOutlineColor);
        }

        void DrawBlock(const Grid& grid, const Block& block)
        {
            const WorldPosition center = grid.CellToWorld(block.cell);
            Color color;
            switch (block.type)
            {
            case BlockType::Grass:
                color = { 82, 145, 66, 255 };
                break;
            case BlockType::Dirt:
                color = { 139, 94, 60, 255 };
                break;
            case BlockType::Stone:
                color = { 132, 137, 142, 255 };
                break;
            default:
                throw std::invalid_argument("Block has an unsupported type.");
            }

            const float cellSize = grid.GetCellSize();
            DrawCube(
                { center.x, center.y, center.z },
                cellSize,
                cellSize,
                cellSize,
                color);
        }

        void DrawItem(const Grid& grid, const WorldObject& object)
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

            const float logLength = cellSize * 0.62f;
            for (int log = 0; log < object.item->GetQuantity(); ++log)
            {
                const float offset =
                    (static_cast<float>(log) - static_cast<float>(object.item->GetQuantity() - 1) * 0.5f) *
                    cellSize * 0.14f;
                DrawCylinderEx(
                    { center.x - logLength * 0.5f, center.y + cellSize * 0.12f + offset, center.z },
                    { center.x + logLength * 0.5f, center.y + cellSize * 0.12f + offset, center.z },
                    cellSize * 0.09f,
                    cellSize * 0.09f,
                    8,
                    { 145, 95, 55, 255 });
            }
        }

        void DrawWorldObject(const Grid& grid, const WorldObject& object)
        {
            const WorldPosition cellCenter = object.position;
            const float cellSize = grid.GetCellSize();

            if (object.type == WorldObjectType::Worker)
            {
                DrawCylinder(
                    { cellCenter.x, cellCenter.y + cellSize * 0.38f, cellCenter.z },
                    cellSize * 0.24f,
                    cellSize * 0.24f,
                    cellSize * 0.76f,
                    8,
                    { 75, 135, 220, 255 });
                DrawSphere(
                    { cellCenter.x, cellCenter.y + cellSize * 0.93f, cellCenter.z },
                    cellSize * 0.20f,
                    { 225, 190, 150, 255 });
                return;
            }

            if (object.type == WorldObjectType::Tree)
            {
                DrawCylinder(
                    { cellCenter.x, cellCenter.y + cellSize * 0.43f, cellCenter.z },
                    cellSize * 0.13f,
                    cellSize * 0.17f,
                    cellSize * 0.86f,
                    8,
                    { 115, 75, 45, 255 });
                DrawSphere(
                    { cellCenter.x, cellCenter.y + cellSize * 1.38f, cellCenter.z },
                    cellSize * 0.48f,
                    { 65, 145, 75, 255 });
                return;
            }

            DrawItem(grid, object);
        }

        void DrawObjectOutline(const Grid& grid, const WorldObject& object, Color color)
        {
            const WorldPosition center = object.position;
            const WorldObjectSize size = GetWorldObjectSize(object.type, grid.GetCellSize());

            DrawBoundingBox(
                {
                    { center.x - size.width * 0.5f, center.y, center.z - size.width * 0.5f },
                    { center.x + size.width * 0.5f, center.y + size.height, center.z + size.width * 0.5f }
                },
                color);
        }

        void DrawDebugPath(const Grid& grid, WorldPosition origin, const std::vector<GridCell>& path)
        {
            if (path.empty())
            {
                return;
            }

            constexpr float pathHeight = 0.075f;
            const float bendRadius = grid.GetCellSize() * 0.12f;
            std::vector<Vector3> points;
            points.reserve(path.size() + 1);
            points.push_back({ origin.x, origin.y + pathHeight, origin.z });
            for (const GridCell cell : path)
            {
                const WorldPosition position = grid.CellToSurface(cell);
                points.push_back({ position.x, position.y + pathHeight, position.z });
            }

            for (std::size_t index = 1; index < points.size(); ++index)
            {
                DrawLine3D(points[index - 1], points[index], kDebugPathColor);
            }

            for (std::size_t index = 1; index + 1 < points.size(); ++index)
            {
                const Vector3 incoming{
                    points[index].x - points[index - 1].x,
                    0.0f,
                    points[index].z - points[index - 1].z
                };
                const Vector3 outgoing{
                    points[index + 1].x - points[index].x,
                    0.0f,
                    points[index + 1].z - points[index].z
                };
                if (std::abs(incoming.x * outgoing.z - incoming.z * outgoing.x) > 1.0e-5f)
                {
                    DrawSphere(
                        { points[index].x, pathHeight + bendRadius, points[index].z },
                        bendRadius,
                        kDebugPathBendColor);
                }
            }
        }
    }

    WorldRenderer::WorldRenderer(CameraConfig config)
        : m_camera(CreateCamera(config))
    {
    }

    void WorldRenderer::Draw(
        const Grid& grid,
        const World& world,
        std::optional<GridCell> hoveredCell,
        std::optional<WorldObjectId> hoveredObject,
        std::optional<WorldObjectId> selectedObject,
        WorldPosition pathOrigin,
        const std::vector<GridCell>& debugPath) const
    {
        BeginMode3D(m_camera);
        for (const Block& block : world.GetBlocks())
        {
            DrawBlock(grid, block);
        }
        if (hoveredCell.has_value())
        {
            DrawHoveredCell(grid, *hoveredCell);
        }
        for (const WorldObject& object : world.GetObjects())
        {
            DrawWorldObject(grid, object);
        }
        DrawGridLines(grid);
        DrawDebugPath(grid, pathOrigin, debugPath);
        for (const WorldObject& object : world.GetObjects())
        {
            const bool isSelected = selectedObject.has_value() && object.id == *selectedObject;
            const bool isHovered = hoveredObject.has_value() && object.id == *hoveredObject;
            if (isSelected)
            {
                DrawObjectOutline(grid, object, kSelectedObjectOutlineColor);
            }
            else if (isHovered)
            {
                DrawObjectOutline(grid, object, kHoveredObjectOutlineColor);
            }
        }
        EndMode3D();
    }

    Camera3D& WorldRenderer::GetCamera()
    {
        return m_camera;
    }

    const Camera3D& WorldRenderer::GetCamera() const
    {
        return m_camera;
    }
}
