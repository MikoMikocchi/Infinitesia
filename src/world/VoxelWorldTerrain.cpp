#include <set>
#include <stdexcept>

#include "world/VoxelWorld.h"
#include "world/internal/WorldTerrainGen.h"

namespace Infinitesia {
namespace {
std::size_t GetBlockIndexHelper(const WorldChunk& chunk, GridCell cell) {
  return WorldInternal::GetBlockIndex(chunk, cell);
}
}  // namespace

const Block* VoxelWorld::GetBlock(GridCell cell) const {
  if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 ||
      cell.y >= m_gridHeight || cell.z < 0 || cell.z >= m_gridDepth) {
    return nullptr;
  }
  const auto chunk = m_chunks.find(GetChunkCoordinate(cell));
  if (chunk == m_chunks.end()) {
    return nullptr;
  }
  return &chunk->second.blocks[GetBlockIndexHelper(chunk->second, cell)];
}

BlockState VoxelWorld::GroundStateAt(GridCell cell) const {
  if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 ||
      cell.y >= m_gridHeight || cell.z < 0 || cell.z >= m_gridDepth) {
    return {};
  }
  return m_terrain->GroundStateAt(cell);
}

bool VoxelWorld::HasSkyAccess(GridCell cell) const {
  if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 ||
      cell.y >= m_gridHeight || cell.z < 0 || cell.z >= m_gridDepth) {
    return false;
  }

  for (int y = cell.y + 1; y < m_gridHeight; ++y) {
    const GridCell above{cell.x, y, cell.z};
    const auto chunk = m_chunks.find(GetChunkCoordinate(above));
    const BlockState state =
        chunk == m_chunks.end()
            ? m_terrain->StateAt(above)
            : chunk->second.blocks[GetBlockIndexHelper(chunk->second, above)]
                  .state;
    // Air and water let the light through; anything that stops it shades the
    // cell.
    if (!m_terrain->Registry().LetsLightThrough(state)) {
      return false;
    }
  }
  return true;
}

void VoxelWorld::SetBlock(GridCell cell, BlockState state) {
  if (m_terrain->Registry().GetMatter(state.matter) == nullptr ||
      m_terrain->Registry().GetShape(state.shape) == nullptr) {
    throw std::invalid_argument(
        "The block registry does not know the material or shape of the block.");
  }
  if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 ||
      cell.y >= m_gridHeight || cell.z < 0 || cell.z >= m_gridDepth) {
    throw std::out_of_range("Block cell is outside the grid.");
  }
  EnsureChunkGenerated(GetChunkCoordinate(cell));
  const ChunkCoordinate changedChunk = GetChunkCoordinate(cell);
  WorldChunk& chunk = m_chunks.at(changedChunk);
  Block& changed = chunk.blocks[GetBlockIndexHelper(chunk, cell)];
  changed.state = state;
  ++m_terrainRevision;

  // A mesh depends on the blocks one cell around its own (hidden faces and the
  // ambient occlusion of their corners, diagonals included), so every chunk
  // holding a cell of the 3x3x3 block around the changed one is stale.
  std::set<ChunkCoordinate> meshNeighbors;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dx = -1; dx <= 1; ++dx) {
        const GridCell around{cell.x + dx, cell.y + dy, cell.z + dz};
        if (Contains(around)) {
          meshNeighbors.insert(GetChunkCoordinate(around));
        }
      }
    }
  }

  for (auto& [coordinate, affectedChunk] : m_chunks) {
    // Light from the sky: the chunks below in the same column.
    const bool affectedByLighting = coordinate.x == changedChunk.x &&
                                    coordinate.z == changedChunk.z &&
                                    coordinate.y <= changedChunk.y;

    if (affectedByLighting || meshNeighbors.count(coordinate) != 0) {
      ++affectedChunk.terrainRevision;
    }
  }
}

std::uint64_t VoxelWorld::GetTerrainRevision() const {
  return m_terrainRevision;
}
}  // namespace Infinitesia
