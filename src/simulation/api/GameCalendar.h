#pragma once

#include <cstdint>

namespace Infinitesia
{
    // A moment of game time, counted from the first tick.
    struct GameTime
    {
        std::uint64_t seconds = 0;     // since the start
        std::uint64_t day = 0;         // whole days since the start
        std::uint64_t month = 0;       // whole months since the start
        std::uint32_t secondOfDay = 0;
        std::uint32_t dayOfMonth = 0;  // 0-based
    };

    // How ticks turn into game time. All integers, and a pure function of the tick number:
    // the calendar keeps no clock of its own, the fixed timestep of `Simulation` is the only one.
    // Tick `t` covers game seconds [t * secondsPerTick, (t + 1) * secondsPerTick).
    class GameCalendar
    {
    public:
        // Throws std::invalid_argument when any value is zero.
        explicit GameCalendar(std::uint32_t secondsPerTick = 60,
                              std::uint32_t secondsPerDay = 86400,
                              std::uint32_t daysPerMonth = 30);

        std::uint32_t SecondsPerTick() const { return m_secondsPerTick; }
        std::uint32_t SecondsPerDay() const { return m_secondsPerDay; }
        std::uint32_t DaysPerMonth() const { return m_daysPerMonth; }
        std::uint64_t SecondsPerMonth() const;

        // Game seconds elapsed once `tickCount` ticks have run.
        std::uint64_t SecondsAfterTicks(std::uint64_t tickCount) const;
        GameTime At(std::uint64_t seconds) const;

    private:
        std::uint32_t m_secondsPerTick;
        std::uint32_t m_secondsPerDay;
        std::uint32_t m_daysPerMonth;
    };
}
