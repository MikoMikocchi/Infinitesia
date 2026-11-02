#pragma once

#include "app/ApplicationConfig.h"
#include "core/Simulation.h"
#include "core/Time.h"
#include "input/CameraController.h"
#include "input/WorldInteractionController.h"
#include "rendering/WorldRenderer.h"
#include "ui/DebugOverlay.h"
#include "commands/CommandRegistry.h"
#include "world/Grid.h"
#include "world/WorkSystem.h"

namespace Infinitesia
{
    namespace AppInternal
    {
        void UpdateLevelSelection(
            WorldRenderer& worldRenderer,
            Grid& grid,
            int& selectedLevel);
    }
}
