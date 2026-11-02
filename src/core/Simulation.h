#pragma once

namespace Infinitesia
{
    class Simulation
    {
    public:
        static void Initialize(float ticksPerSecond = 20.0f);
        static void AdvanceFrame(float frameDelta);
        static void Tick();

        static int GetTickCount();
        static float GetFixedDeltaTime();
        static float GetTicksPerSecond();

    private:
        static float m_ticksPerSecond;
        static float m_fixedDeltaTime;
        static float m_accumulator;
        static int m_tickCount;
    };
}
