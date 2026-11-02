#pragma once

#include "raylib.h"
#include "world/Grid.h"

#include <optional>

namespace Infinitesia
{
    struct CameraConfig
    {
        float distance;
        float elevationDegrees;
        float fieldOfViewDegrees;
    };

    class WorldRenderer
    {
    public:
        explicit WorldRenderer(CameraConfig config);

        void Draw(const Grid& grid, std::optional<GridCell> hoveredCell) const;
        Camera3D& GetCamera();
        const Camera3D& GetCamera() const;

    private:
        Camera3D m_camera;
    };
}
