#pragma once

#include "presentation/input/CameraController.h"
#include "presentation/rendering/WorldRenderer.h"
#include "world/Grid.h"
#include "gameplay/work/WorkSystem.h"

#include <cstdint>
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
        // The world of the founded settlement: a 256 m square window of the planet, 192 m
        // tall, with the ground of its middle column at level 112.
        GridConfig settlementGrid{ 256, 192, 256, 1.0f };
        int settlementAnchorLevel = 112;
        CameraConfig camera{ 30.0f, 55.0f, 45.0f };
        CameraControlConfig cameraControls{ 12.0f, 0.25f, 2.0f, 3.0f, 100.0f, 10.0f, 85.0f };
        WorkerConfig worker{ 3.0f, 4.0f, 3.0f, 7000.0f };
        bool showPathDebug = true;
        float simulationTicksPerSecond = 20.0f;
        std::uint32_t worldSeed = 1;
    };
}
