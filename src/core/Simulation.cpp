#include "core/Simulation.h"

#include <cmath>

namespace Infinitesia
{
    float Simulation::m_ticksPerSecond = 20.0f;
    float Simulation::m_fixedDeltaTime = 0.05f;
    float Simulation::m_accumulator = 0.0f;
    int Simulation::m_tickCount = 0;

    void Simulation::Initialize(float ticksPerSecond)
    {
        if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.0f)
        {
            ticksPerSecond = 20.0f;
        }

        m_ticksPerSecond = ticksPerSecond;
        m_fixedDeltaTime = 1.0f / m_ticksPerSecond;
        m_accumulator = 0.0f;
        m_tickCount = 0;
    }

    void Simulation::AdvanceFrame(float frameDelta)
    {
        m_accumulator += frameDelta;

        while (m_accumulator >= m_fixedDeltaTime)
        {
            Tick();
            m_accumulator -= m_fixedDeltaTime;
        }
    }

    void Simulation::Tick()
    {
        // Логика симуляции будет здесь
        ++m_tickCount;
    }

    int Simulation::GetTickCount()
    {
        return m_tickCount;
    }

    float Simulation::GetFixedDeltaTime()
    {
        return m_fixedDeltaTime;
    }

    float Simulation::GetTicksPerSecond()
    {
        return m_ticksPerSecond;
    }
}
