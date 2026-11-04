#pragma once

#include <cstdint>

namespace Infinitesia {
// Dense index of a material: what a block of the world is made of. Ranges
// (docs/06_BLOCKS.md, section 3.1): 0-31 air and fluids, 32-511 rocks, 512-1023
// loose sediments, soils and ice, 1024-2047 built materials, 2048-4095
// minerals, 4096-5119 plants. The numbers are the stable numeric IDs of the
// catalog records and are never reused.
using MatterId = std::uint16_t;

inline constexpr MatterId kAirMatter = 0;
inline constexpr MatterId kWaterMatter = 1;
inline constexpr MatterId kFirstRockMatter = 32;
inline constexpr MatterId kFirstLooseMatter = 512;
inline constexpr MatterId kFirstBuiltMatter = 1024;
inline constexpr MatterId kFirstMineralMatter = 2048;
inline constexpr MatterId kFirstPlantMatter = 4096;
inline constexpr int kMatterCapacity = 5120;

// "No material": what a record holds when it names none (an event that formed
// no rock).
inline constexpr MatterId kUnspecifiedMatter = 0xFFFF;
}  // namespace Infinitesia
