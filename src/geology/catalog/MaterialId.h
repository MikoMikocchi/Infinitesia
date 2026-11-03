#pragma once

#include <cstdint>

namespace Infinitesia
{
    // Dense index of a material: what a block of the world is made of. Ranges
    // (docs/03_GEOLOGY.md, section 18): 0-31 air and fluids, 32-511 rocks, 512-1023
    // loose sediments, soils and ice, 1024+ reserved. The numbers are the stable
    // numeric IDs of the catalog records and are never reused.
    using MaterialId = std::uint16_t;

    inline constexpr MaterialId kAirMaterial = 0;
    inline constexpr MaterialId kWaterMaterial = 1;
    inline constexpr MaterialId kFirstRockMaterial = 32;
    inline constexpr MaterialId kFirstLooseMaterial = 512;
    inline constexpr MaterialId kFirstReservedMaterial = 1024;

    // A block that carries no catalog material (legacy blocks, tests): its BlockType decides.
    inline constexpr MaterialId kUnspecifiedMaterial = 0xFFFF;
}
