#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"

#include <optional>
#include <vector>

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

        void Draw(
            const Grid& grid,
            const World& world,
            std::optional<GridCell> hoveredCell,
            std::optional<WorldObjectId> hoveredObject,
            std::optional<WorldObjectId> selectedObject,
            WorldPosition pathOrigin,
            const std::vector<GridCell>& debugPath) const;
        Camera3D& GetCamera();
        const Camera3D& GetCamera() const;

    private:
        Camera3D m_camera;
    };
}
