#pragma once

#include "simulation/api/GameCalendar.h"

#include <cstdint>

namespace Infinitesia
{
    enum class CadenceUnit : std::uint8_t
    {
        Tick,
        GameSecond,
        GameDay,
        GameMonth,
    };

    // How often a system runs: every `interval` units of `unit`.
    //
    // A system is due on the tick in which a multiple of its interval is reached (counting from
    // the start of the game, at the end of the tick), so every 3 ticks means ticks 2, 5, 8, ...
    // and every day means the tick that crosses midnight. At most once per tick: an interval
    // shorter than a tick runs every tick, it does not catch up.
    struct Cadence
    {
        CadenceUnit unit = CadenceUnit::Tick;
        std::uint64_t interval = 1;

        static constexpr Cadence EveryTick() { return { CadenceUnit::Tick, 1 }; }
        static constexpr Cadence EveryTicks(std::uint64_t ticks) { return { CadenceUnit::Tick, ticks }; }
        static constexpr Cadence EveryGameSeconds(std::uint64_t seconds) { return { CadenceUnit::GameSecond, seconds }; }
        static constexpr Cadence EveryGameDays(std::uint64_t days) { return { CadenceUnit::GameDay, days }; }
        static constexpr Cadence EveryGameMonths(std::uint64_t months) { return { CadenceUnit::GameMonth, months }; }

        bool IsValid() const { return interval > 0; }
    };

    // Whether a system with this cadence runs on tick `tick` (0-based). The cadence must be valid.
    bool IsDue(const Cadence& cadence, const GameCalendar& calendar, std::uint64_t tick);
}
