#pragma once

#include "raylib.h"
#include "world/Grid.h"

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

        void Draw(const Grid& grid) const;

    private:
        Camera3D m_camera;
    };
}
