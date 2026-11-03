#pragma once

#include "raylib.h"

namespace Infinitesia
{
    struct CameraControlConfig
    {
        float movementSpeed;
        float rotationSensitivity;
        float zoomSpeed;
        float minimumDistance;
        float maximumDistance;
        float minimumElevationDegrees;
        float maximumElevationDegrees;
    };

    class CameraController
    {
    public:
        explicit CameraController(CameraControlConfig config);

        void Update(Camera3D& camera, float deltaTime) const;

    private:
        CameraControlConfig m_config;
    };
}
