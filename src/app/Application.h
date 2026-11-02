#pragma once

namespace Infinitesia
{
    class Application
    {
    public:
        void Run();

    private:
        void Initialize();
        void UpdateFrame();
        void Render();
        void DrawDebugOverlay();
        void Shutdown();

        const int m_screenWidth = 1280;
        const int m_screenHeight = 720;
        const char* m_windowTitle = "Infinitesia";
    };
}
