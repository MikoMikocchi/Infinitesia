#include "app/Application.h"
#include "core/Time.h"
#include "raylib.h"

#include <cstdio>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        class WindowGuard
        {
        public:
            WindowGuard(int width, int height, const char* title)
            {
                InitWindow(width, height, title);
                if (!IsWindowReady())
                {
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
        WindowGuard window(m_screenWidth, m_screenHeight, m_windowTitle);
        SetTargetFPS(60);

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
        ClearBackground(RAYWHITE);

        DrawText("Infinitesia", m_screenWidth / 2 - MeasureText("Infinitesia", 40) / 2, m_screenHeight / 2 - 20, 40, DARKGRAY);
        
        DrawDebugOverlay();

        EndDrawing();
    }

    void Application::DrawDebugOverlay()
    {
        const int padding = 8;
        const int fontSize = 16;
        const int lineHeight = 20;

        char buf[128];

        DrawRectangle(padding - 4, padding - 4, 250, 3 * lineHeight + padding, Fade(BLACK, 0.4f));

        std::snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
        DrawText(buf, padding, padding, fontSize, LIME);

        std::snprintf(buf, sizeof(buf), "Time since start: %.3f s", m_time.GetTotalTime());
        DrawText(buf, padding, padding + lineHeight, fontSize, SKYBLUE);

        std::snprintf(buf, sizeof(buf), "Ticks: %llu", static_cast<unsigned long long>(m_simulation.GetTickCount()));
        DrawText(buf, padding, padding + 2 * lineHeight, fontSize, ORANGE);
    }
}
