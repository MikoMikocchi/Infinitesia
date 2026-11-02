#pragma once

#include "rendering/WorldRenderer.h"
#include "world/Grid.h"

#include <string>

namespace Infinitesia
{
    struct WindowConfig
    {
        int width;
        int height;
        int targetFramesPerSecond;
        std::string title;
    };

    struct ApplicationConfig
    {
        WindowConfig window{ 1280, 720, 60, "Infinitesia" };
        GridConfig grid{ 24, 24, 1.0f };
        CameraConfig camera{ 30.0f, 55.0f, 45.0f };
        float simulationTicksPerSecond = 20.0f;
    };
}
