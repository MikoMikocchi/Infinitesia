#include "rendering/WorldOverlayRenderer.h"

#include <cmath>
#include <cstdint>

namespace Infinitesia
{
    namespace WorldOverlayRenderer
    {
        namespace
        {
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
            constexpr float kDebugPathHeight = 0.075f;

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

        void DrawDebugPath(const Grid& grid, WorldPosition origin, const std::vector<GridCell>& path)
        {
            if (path.empty())
            {
                return;
            }

            const float bendRadius = grid.GetCellSize() * 0.12f;
            std::vector<Vector3> points;
            points.reserve(path.size() + 1);
            points.push_back({ origin.x, origin.y + kDebugPathHeight, origin.z });
            for (const GridCell cell : path)
            {
                const WorldPosition position = grid.CellToSurface(cell);
                points.push_back({
                    position.x,
                    position.y + kDebugPathHeight,
                    position.z
                });
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
                        { points[index].x, kDebugPathHeight + bendRadius, points[index].z },
                        bendRadius,
                        kDebugPathBendColor);
                }
            }
        }

        void DrawObjectHighlights(
            const Grid& grid,
            const World& world,
            std::optional<WorldObjectId> hoveredObject,
            std::optional<WorldObjectId> selectedObject)
        {
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
        }
    }
}
