#pragma once

#include <cstdint>

namespace Infinitesia
{
    // Dense index of a material: what a block of the world is made of. Ranges
    // (docs/06_BLOCKS.md, section 3.1): 0-31 air and fluids, 32-511 rocks, 512-1023 loose
    // sediments, soils and ice, 1024-2047 built materials, 2048-4095 minerals, 4096-5119 plants. The numbers are the stable
    // numeric IDs of the catalog records and are never reused.
    using MaterialId = std::uint16_t;

    inline constexpr MaterialId kAirMaterial = 0;
    inline constexpr MaterialId kWaterMaterial = 1;
    inline constexpr MaterialId kFirstRockMaterial = 32;
    inline constexpr MaterialId kFirstLooseMaterial = 512;
    inline constexpr MaterialId kFirstBuiltMaterial = 1024;
    inline constexpr MaterialId kFirstMineralMaterial = 2048;
    inline constexpr MaterialId kFirstPlantMaterial = 4096;
    inline constexpr int kMaterialCapacity = 5120;

    // "No material": what a record holds when it names none (an event that formed no rock).
    inline constexpr MaterialId kUnspecifiedMaterial = 0xFFFF;
}
