#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace Infinitesia
{
    // Tags the engine itself asks about (rules ask "is it water", not "is it id 1"). Data may carry
    // other tags too; those are kept as text and checked with BlockRegistry::HasTag.
    enum class Tag : std::uint8_t
    {
        Air,
        Fluid,
        Water,
        Soil,
        Rock,
        Ice,
        Built,
        Glass,
        LightSource,
    };

    inline constexpr int kEngineTagCount = 9;

    std::string_view TagName(Tag tag) noexcept;
    std::optional<Tag> TagFromName(std::string_view name) noexcept;
}
