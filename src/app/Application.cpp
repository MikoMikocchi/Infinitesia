#include "Application.h"

#include <string>

namespace Infinitesia
{
    Application::Application()
    {
    }

    Application::~Application()
    {
    }

    void Application::Run()
    {
        Init();
        while (!WindowShouldClose())
        {
            Update();
            Draw();
        }
        Shutdown();
    }

    void Application::Init()
    {
        InitWindow(m_screenWidth, m_screenHeight, m_windowTitle);
        SetTargetFPS(60);
    }

    void Application::Update()
    {
        // Simulation/update logic will go here
    }

    void Application::Draw()
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Infinitesia", m_screenWidth / 2 - MeasureText("Infinitesia", 40) / 2, m_screenHeight / 2 - 20, 40, DARKGRAY);
        EndDrawing();
    }

    void Application::Shutdown()
    {
        CloseWindow();
    }
}
