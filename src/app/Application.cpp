#include "app/Application.h"
#include "raylib.h"

#include <cstdio>
#include <stdexcept>
#include <utility>

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

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_grid(m_config.grid),
          m_simulation(m_config.simulationTicksPerSecond),
          m_worldRenderer(m_config.camera)
    {
    }

    void Application::Run()
    {
        WindowGuard window(m_config.window);
        SetTargetFPS(m_config.window.targetFramesPerSecond);

        while (!WindowShouldClose())
        {
            m_time.Advance(GetFrameTime());
            m_simulation.AdvanceFrame(m_time.GetDeltaTime());
            Render();
        }
    }

    void Application::Render()
    {
        BeginDrawing();
        ClearBackground({ 30, 34, 39, 255 });

        m_worldRenderer.Draw(m_grid);
        DrawText(
            m_config.window.title.c_str(),
            (m_config.window.width - MeasureText(m_config.window.title.c_str(), 24)) / 2,
            12,
            24,
            RAYWHITE);
        DrawDebugOverlay();

        EndDrawing();
    }

    void Application::DrawDebugOverlay()
    {
        const int padding = 8;
        const int fontSize = 16;
        const int lineHeight = 20;
        const int panelWidth = 250;
        const int panelHeight = 4 * lineHeight + padding;

        char buf[128];

        DrawRectangle(padding - 4, padding - 4, panelWidth, panelHeight, Fade(BLACK, 0.4f));

        std::snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
        DrawText(buf, padding, padding, fontSize, LIME);

        std::snprintf(buf, sizeof(buf), "Time since start: %.3f s", m_time.GetTotalTime());
        DrawText(buf, padding, padding + lineHeight, fontSize, SKYBLUE);

        std::snprintf(buf, sizeof(buf), "Ticks: %llu", static_cast<unsigned long long>(m_simulation.GetTickCount()));
        DrawText(buf, padding, padding + 2 * lineHeight, fontSize, ORANGE);

        std::snprintf(buf, sizeof(buf), "Grid: %d x %d", m_grid.GetColumns(), m_grid.GetRows());
        DrawText(buf, padding, padding + 3 * lineHeight, fontSize, RAYWHITE);
    }
}
