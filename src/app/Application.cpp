#include "app/Application.h"
#include "core/Time.h"
#include "core/Simulation.h"
#include "raylib.h"

#include <cstdio>

namespace Infinitesia
{
    void Application::Run()
    {
        Initialize();
        while (!WindowShouldClose())
        {
            Time::UpdateFrameTime();
            Simulation::AdvanceFrame(Time::GetDeltaTime());
            UpdateFrame();
            Render();
        }
        Shutdown();
    }

    void Application::Initialize()
    {
        InitWindow(m_screenWidth, m_screenHeight, m_windowTitle);
        SetTargetFPS(60);
        Simulation::Initialize(20.0f);
    }

    void Application::UpdateFrame()
    {
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

        std::snprintf(buf, sizeof(buf), "FPS: %d", Time::GetFPS());
        DrawText(buf, padding, padding, fontSize, LIME);

        std::snprintf(buf, sizeof(buf), "Time since start: %.3f s", Time::GetTotalTime());
        DrawText(buf, padding, padding + lineHeight, fontSize, SKYBLUE);

        std::snprintf(buf, sizeof(buf), "Ticks: %d", Simulation::GetTickCount());
        DrawText(buf, padding, padding + 2 * lineHeight, fontSize, ORANGE);
    }

    void Application::Shutdown()
    {
        CloseWindow();
    }
}
