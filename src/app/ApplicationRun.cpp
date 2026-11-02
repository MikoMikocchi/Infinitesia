#include "app/Application.h"
#include "raylib.h"

#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        class WindowGuard
        {
        public:
            explicit WindowGuard(const WindowConfig& config)
            {
                if (config.width <= 0 || config.height <= 0 || config.title.empty() ||
                    config.targetFramesPerSecond < 0)
                {
                    throw std::invalid_argument("Window configuration contains invalid values.");
                }
                InitWindow(config.width, config.height, config.title.c_str());
                if (!IsWindowReady())
                {
                    CloseWindow();
                    throw std::runtime_error("Failed to initialize the application window.");
                }
            }

            ~WindowGuard()
            {
                CloseWindow();
            }

            WindowGuard(const WindowGuard&) = delete;
            WindowGuard& operator=(const WindowGuard&) = delete;
        };
    }

    void Application::Run()
    {
        WindowGuard window(m_config.window);
        SetTargetFPS(m_config.window.targetFramesPerSecond);

        try
        {
            while (!WindowShouldClose())
            {
                m_time.Advance(GetFrameTime());
                m_simulation.AdvanceFrame(m_time.GetDeltaTime());
                Update();
                Render();
            }
        }
        catch (...)
        {
            m_planetViewer.ReleaseRenderResources();
            throw;
        }
        m_planetViewer.ReleaseRenderResources();
    }
}
