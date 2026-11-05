#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/Block.h"

namespace Infinitesia {
inline constexpr int kChunkSize = 16;

struct ChunkCoordinate {
  int x;
  int y;
  int z;

  bool operator==(const ChunkCoordinate&) const = default;
  bool operator<(const ChunkCoordinate& other) const {
    if (x != other.x) {
      return x < other.x;
    }
    if (y != other.y) {
      return y < other.y;
    }
    return z < other.z;
  }
};

struct WorldChunk {
  ChunkCoordinate coordinate;
  int width;
  int height;
  int depth;
  std::uint64_t terrainRevision = 0;
  // Whether any cell holds a fluid (water, oil, magma): such a chunk stays
  // visible above the cut level, since the sea is not cut away with the ground.
  bool hasFluid = false;
  std::vector<Block> blocks;
};
}  // namespace Infinitesia
