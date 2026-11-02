#include "rendering/WorldRenderer.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr float kDegreesToRadians = PI / 180.0f;
        constexpr float kGroundPlaneOffset = 0.01f;
        constexpr float kGridLineHeight = 0.01f;
        constexpr Color kGroundColor = { 48, 54, 60, 255 };
        constexpr Color kGridLineColor = { 85, 95, 105, 255 };
        constexpr Color kGridBoundaryColor = { 145, 155, 165, 255 };

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
    }

    WorldRenderer::WorldRenderer(CameraConfig config)
        : m_camera(CreateCamera(config))
    {
    }

    void WorldRenderer::Draw(const Grid& grid) const
    {
        BeginMode3D(m_camera);
        DrawPlane(
            { 0.0f, -kGroundPlaneOffset, 0.0f },
            { grid.GetWorldWidth(), grid.GetWorldDepth() },
            kGroundColor);
        DrawGridLines(grid);
        EndMode3D();
    }
}
