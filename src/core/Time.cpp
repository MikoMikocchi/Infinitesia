#include "core/Time.h"
#include "raylib.h"

namespace Infinitesia
{
    float Time::m_deltaTime = 0.0f;
    double Time::m_totalTime = 0.0;

    void Time::UpdateFrameTime()
    {
        m_deltaTime = GetFrameTime();
        m_totalTime += static_cast<double>(m_deltaTime);
    }

    float Time::GetDeltaTime()
    {
        return m_deltaTime;
    }

    double Time::GetTotalTime()
    {
        return m_totalTime;
    }

    double Time::GetTotalTimeMilliseconds()
    {
        return m_totalTime * 1000.0;
    }

    int Time::GetFPS()
    {
        return ::GetFPS();
    }
}
