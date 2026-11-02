#pragma once

#include <cstdint>

namespace Infinitesia
{
    class Simulation
    {
    public:
        explicit Simulation(float ticksPerSecond = 20.0f);

        void AdvanceFrame(float frameDelta);

        std::uint64_t GetTickCount() const;
        float GetFixedDeltaTime() const;
        float GetTicksPerSecond() const;

    private:
        void Tick();

        float m_ticksPerSecond;
        float m_fixedDeltaTime;
        float m_accumulator = 0.0f;
        std::uint64_t m_tickCount = 0;
    };
}
