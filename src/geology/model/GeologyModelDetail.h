#pragma once

#include <numbers>

// Constants and helpers shared by the translation units that implement GeologyModel.
namespace Infinitesia::GeologyModelDetail
{
    constexpr double kDegrees = std::numbers::pi / 180.0;

    constexpr double Lerp(double first, double second, double amount) noexcept
    {
        return first + (second - first) * amount;
    }
}
