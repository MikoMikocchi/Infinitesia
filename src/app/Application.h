#pragma once

#include "app/ApplicationConfig.h"
#include "core/Simulation.h"
#include "core/Time.h"
#include "input/GridPointer.h"
#include "rendering/WorldRenderer.h"
#include "world/Grid.h"

#include <optional>

namespace Infinitesia
{
    class Application
    {
    public:
        explicit Application(ApplicationConfig config = {});

        void Run();

    private:
        void Update();
        void Render();
        void DrawDebugOverlay();

        ApplicationConfig m_config;
        Grid m_grid;
        Simulation m_simulation;
        WorldRenderer m_worldRenderer;
        GridPointer m_gridPointer;
        std::optional<GridCell> m_hoveredCell;
        Time m_time;
    };
}
