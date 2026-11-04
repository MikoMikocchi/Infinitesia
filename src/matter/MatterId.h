#pragma once

#include <cstdint>

namespace Infinitesia {
// Dense index of a material: what a block of the world is made of. The catalog
// assigns it while loading, in the order the records are written (air first,
// then the fluids), so it is 0, 1, 2, ... and is the material's place in the
// catalog (docs/DataDriven.md, section 4.2). The data never writes these
// numbers: a definition is named by its stable string id, and only the runtime
// keeps the index.
using MatterId = std::uint16_t;

// The two indices the game names directly: air is the empty cell, water the
// first fluid. Both hold as long as the catalog keeps air first and a fluid
// second.
inline constexpr MatterId kAirMatter = 0;
inline constexpr MatterId kWaterMatter = 1;

// The most materials a catalog can hold (the index is 16-bit).
inline constexpr int kMatterCapacity = 5120;

// "No material": what a record holds when it names none (an event that formed
// no rock).
inline constexpr MatterId kUnspecifiedMatter = 0xFFFF;
}  // namespace Infinitesia
