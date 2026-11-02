#pragma once

#include "core/Simulation.h"
#include "core/Time.h"

namespace Infinitesia
{
    class Application
    {
    public:
        void Run();

    private:
        void Render();
        void DrawDebugOverlay();

        static constexpr int m_screenWidth = 1280;
        static constexpr int m_screenHeight = 720;
        static constexpr const char* m_windowTitle = "Infinitesia";

        Time m_time;
        Simulation m_simulation;
    };
}
