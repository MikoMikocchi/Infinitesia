#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <utility>

#include "world/VoxelWorld.h"

namespace Infinitesia {
namespace WorldGeometryRenderer {
// Finds the chunks buried under the ground that the view cannot reach: seen
// from above, a chunk is hidden when every column of it, and of the ring around
// it, has a solid block over the chunk's top that is not past the view level.
class TerrainOcclusion {
 public:
  // `topLevel` is the highest level drawn.
  bool IsHidden(const VoxelWorld& world, const WorldChunk& chunk, int topLevel);

 private:
  using ColumnKey = std::pair<int, int>;
  // The highest solid level of each cell of a chunk column at or under the top
  // level; -1 when there is none or the chunks above are not generated yet.
  using ColumnTops = std::array<int, kChunkSize * kChunkSize>;

  const ColumnTops& GetColumn(const VoxelWorld& world, ColumnKey key,
                              int topLevel);

  const VoxelWorld* m_world = nullptr;
  int m_topLevel = -1;
  std::uint64_t m_terrainRevision = 0;
  std::size_t m_chunkCount = 0;
  std::map<ColumnKey, ColumnTops> m_columns;
};
}  // namespace WorldGeometryRenderer
}  // namespace Infinitesia
