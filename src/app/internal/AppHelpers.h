#pragma once

#include "app/ApplicationConfig.h"
#include "foundation/Simulation.h"
#include "foundation/Time.h"
#include "presentation/input/CameraController.h"
#include "presentation/input/WorldInteractionController.h"
#include "presentation/rendering/WorldRenderer.h"
#include "presentation/ui/DebugOverlay.h"
#include "gameplay/commands/CommandRegistry.h"
#include "world/Grid.h"
#include "gameplay/work/WorkSystem.h"

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
