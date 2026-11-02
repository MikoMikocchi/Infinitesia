#pragma once

#include "app/ApplicationConfig.h"
#include "core/Simulation.h"
#include "core/Time.h"
#include "rendering/WorldRenderer.h"
#include "world/Grid.h"

namespace Infinitesia
{
    class Application
    {
    public:
        explicit Application(ApplicationConfig config = {});

        void Run();

    private:
        void Render();
        void DrawDebugOverlay();

        ApplicationConfig m_config;
        Grid m_grid;
        Simulation m_simulation;
        WorldRenderer m_worldRenderer;
        Time m_time;
    };
}
