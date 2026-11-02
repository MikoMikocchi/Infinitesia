#pragma once

#include "input/CameraController.h"
#include "rendering/WorldRenderer.h"
#include "world/Grid.h"
#include "world/World.h"
#include "world/WorkSystem.h"

#include <cstdint>
#include <string>
#include <vector>

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
        WindowConfig window{ 1920, 1080, 60, "Infinitesia" };
        GridConfig grid{ 256, 128, 256, 1.0f };
        CameraConfig camera{ 30.0f, 55.0f, 45.0f };
        CameraControlConfig cameraControls{ 12.0f, 0.25f, 2.0f, 3.0f, 100.0f, 10.0f, 85.0f };
        std::vector<WorldObjectPlacement> initialWorldObjects{
            { WorldObjectType::Worker, { 128, 127, 128 } },
            { WorldObjectType::Tree, { 132, 127, 128 } }
        };
        WorkerConfig worker{ 3.0f, 4.0f, 3 };
        bool showPathDebug = true;
        float simulationTicksPerSecond = 20.0f;
        std::uint32_t worldSeed = 1;
    };
}
