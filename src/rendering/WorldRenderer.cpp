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
        constexpr float kGroundPlaneOffset = 0.01f;
        constexpr float kGridLineHeight = 0.01f;
        constexpr float kHighlightHeight = 0.005f;
        constexpr Color kGroundColor = { 48, 54, 60, 255 };
        constexpr Color kGridLineColor = { 85, 95, 105, 255 };
        constexpr Color kGridBoundaryColor = { 145, 155, 165, 255 };
        constexpr Color kHoveredCellColor = { 245, 190, 55, 100 };
        constexpr Color kHoveredCellOutlineColor = { 255, 215, 90, 255 };

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
            const float lineY = kGridLineHeight;

            for (std::int64_t column = 0; column <= grid.GetColumns(); ++column)
            {
                const float x = -halfWidth + static_cast<float>(column) * grid.GetCellSize();
                const Color color = (column == 0 || column == grid.GetColumns())
                    ? kGridBoundaryColor
                    : kGridLineColor;
                DrawLine3D({ x, lineY, -halfDepth }, { x, lineY, halfDepth }, color);
            }

            for (std::int64_t row = 0; row <= grid.GetRows(); ++row)
            {
                const float z = -halfDepth + static_cast<float>(row) * grid.GetCellSize();
                const Color color = (row == 0 || row == grid.GetRows())
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

            const WorldPosition center = grid.CellToWorld(cell);
            const float halfCell = grid.GetCellSize() * 0.5f;
            const float left = center.x - halfCell;
            const float right = center.x + halfCell;
            const float top = center.z - halfCell;
            const float bottom = center.z + halfCell;

            DrawPlane(
                { center.x, kHighlightHeight, center.z },
                { grid.GetCellSize(), grid.GetCellSize() },
                kHoveredCellColor);
            DrawLine3D({ left, kHighlightHeight, top }, { right, kHighlightHeight, top }, kHoveredCellOutlineColor);
            DrawLine3D({ right, kHighlightHeight, top }, { right, kHighlightHeight, bottom }, kHoveredCellOutlineColor);
            DrawLine3D({ right, kHighlightHeight, bottom }, { left, kHighlightHeight, bottom }, kHoveredCellOutlineColor);
            DrawLine3D({ left, kHighlightHeight, bottom }, { left, kHighlightHeight, top }, kHoveredCellOutlineColor);
        }
    }

    WorldRenderer::WorldRenderer(CameraConfig config)
        : m_camera(CreateCamera(config))
    {
    }

    void WorldRenderer::Draw(const Grid& grid, std::optional<GridCell> hoveredCell) const
    {
        BeginMode3D(m_camera);
        DrawPlane(
            { 0.0f, -kGroundPlaneOffset, 0.0f },
            { grid.GetWorldWidth(), grid.GetWorldDepth() },
            kGroundColor);
        if (hoveredCell.has_value())
        {
            DrawHoveredCell(grid, *hoveredCell);
        }
        DrawGridLines(grid);
        EndMode3D();
    }

    const Camera3D& WorldRenderer::GetCamera() const
    {
        return m_camera;
    }
}
