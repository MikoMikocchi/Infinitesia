#pragma once

#include <cstdint>

namespace Infinitesia::GeologyRandom
{
    // splitmix64 finalizer: a stable, platform-independent 64-bit mix.
    constexpr std::uint64_t Mix(std::uint64_t value) noexcept
    {
        value += 0x9e3779b97f4a7c15ull;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
        return value ^ (value >> 31);
    }

    // An independent value per (seed, stage, entity, index): the geology never draws
    // from a shared generator, so results do not depend on the order things are asked for
    // (docs/03_GEOLOGY.md, section 4.1).
    constexpr std::uint64_t Stream(std::uint64_t seed, std::uint64_t stage, std::uint64_t entity,
                                   std::uint64_t index) noexcept
    {
        return Mix(Mix(Mix(Mix(seed) ^ stage) ^ entity) ^ index);
    }

    // Uniform in [0, 1).
    constexpr double Unit(std::uint64_t hash) noexcept
    {
        return static_cast<double>(hash >> 11) * (1.0 / 9007199254740992.0);
    }
}
