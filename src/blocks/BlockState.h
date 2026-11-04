#pragma once

#include <cstdint>

#include "matter/MatterId.h"

namespace Infinitesia {
// What is built of a material: the shape of the thing in the cell
// (docs/06_BLOCKS.md, section 3). Only the full cube exists so far; walls,
// floors and stairs come later.
using ShapeId = std::uint8_t;
inline constexpr ShapeId kShapeFull = 0;

// Everything the registry knows about a world cell: what it is made of and what
// was made of it.
struct BlockState {
  MatterId matter = kAirMatter;
  ShapeId shape = kShapeFull;

  friend bool operator==(const BlockState&, const BlockState&) = default;
};

inline constexpr int kShapeCapacity = 256;
}  // namespace Infinitesia
