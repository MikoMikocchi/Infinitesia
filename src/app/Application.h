#pragma once

#include "raylib.h"

namespace Infinitesia
{
    class Application
    {
    public:
        Application();
        ~Application();

        void Run();

    private:
        void Init();
        void Update();
        void Draw();
        void Shutdown();

        const int m_screenWidth = 1280;
        const int m_screenHeight = 720;
        const char* m_windowTitle = "Infinitesia";
    };
}
