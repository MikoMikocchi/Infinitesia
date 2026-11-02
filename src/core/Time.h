#pragma once

namespace Infinitesia
{
    class Time
    {
    public:
        static void UpdateFrameTime();
        
        static float GetDeltaTime();
        static double GetTotalTime();
        static double GetTotalTimeMilliseconds();
        static int GetFPS();
        
    private:
        static float m_deltaTime;
        static double m_totalTime;
    };
}
