#pragma once

#include <cstdint>

namespace Infinitesia
{
    struct RgbColor
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;

        friend bool operator==(const RgbColor&, const RgbColor&) = default;
    };

    // Where a value comes from (docs/03_GEOLOGY.md, section 0): a reference work, a
    // derivation from other data, or a game assumption.
    enum class Provenance : std::uint8_t { Reference, Derived, Gameplay };

    // What a block of this material is for gameplay and rendering.
    // Built is everything made by people or by the game's design rather than by geology (planks,
    // glass, lamps); Mineral is a pure mineral (quartz, magnetite) that can fill a cell on its own.
    enum class BlockClass : std::uint8_t { Air, Fluid, Loose, Rock, Ice, Built, Mineral, Plant };

    // Block light is coloured: one 0-15 level per channel. The same shape holds how much a material
    // dims each channel of the light that passes through it (docs/06_BLOCKS.md, section 3.2).
    struct LightRgb
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;

        friend bool operator==(const LightRgb&, const LightRgb&) = default;
    };

    inline constexpr std::uint8_t kMaxLightLevel = 15;

    // How much a material of the class dims light when the data does not say: air passes it,
    // water and ice dim it a little, ground and rock stop it.
    inline constexpr LightRgb DefaultOpacity(BlockClass blockClass)
    {
        switch (blockClass)
        {
        case BlockClass::Air:   return { 0, 0, 0 };
        case BlockClass::Fluid: return { 2, 2, 2 };
        case BlockClass::Ice:   return { 3, 3, 3 };
        case BlockClass::Loose:
        case BlockClass::Rock:
        case BlockClass::Built:
        case BlockClass::Mineral:
        case BlockClass::Plant: break;
        }
        return { kMaxLightLevel, kMaxLightLevel, kMaxLightLevel };
    }

    // Tag every material of the class carries besides the ones written in the data.
    inline constexpr const char* ClassTag(BlockClass blockClass)
    {
        switch (blockClass)
        {
        case BlockClass::Air:     return "air";
        case BlockClass::Fluid:   return "fluid";
        case BlockClass::Loose:   return "loose";
        case BlockClass::Rock:    return "rock";
        case BlockClass::Ice:     return "ice";
        case BlockClass::Built:   return "built";
        case BlockClass::Mineral: return "mineral";
        case BlockClass::Plant:   return "plant";
        }
        return "";
    }

    enum class RockClass : std::uint8_t
    {
        IgneousIntrusive,
        IgneousExtrusive,
        Volcaniclastic,
        SedimentaryClastic,
        SedimentaryChemical,
        SedimentaryBiogenic,
        Metamorphic,
    };
}
