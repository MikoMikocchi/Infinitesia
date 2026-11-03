#include "world/VoxelWorld.h"
#include "world/internal/WorldTerrainGen.h"

#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        std::size_t GetBlockIndexHelper(const WorldChunk& chunk, GridCell cell)
        {
            return WorldInternal::GetBlockIndex(chunk, cell);
        }
    }

    const Block* VoxelWorld::GetBlock(GridCell cell) const
    {
        if (cell.x < 0 || cell.x >= m_gridWidth ||
            cell.y < 0 || cell.y >= m_gridHeight ||
            cell.z < 0 || cell.z >= m_gridDepth)
        {
            return nullptr;
        }
        const auto chunk = m_chunks.find(GetChunkCoordinate(cell));
        if (chunk == m_chunks.end())
        {
            return nullptr;
        }
        return &chunk->second.blocks[GetBlockIndexHelper(chunk->second, cell)];
    }

    bool VoxelWorld::HasSkyAccess(GridCell cell) const
    {
        if (cell.x < 0 || cell.x >= m_gridWidth ||
            cell.y < 0 || cell.y >= m_gridHeight ||
            cell.z < 0 || cell.z >= m_gridDepth)
        {
            return false;
        }

        for (int y = cell.y + 1; y < m_gridHeight; ++y)
        {
            const GridCell above{ cell.x, y, cell.z };
            const auto chunk = m_chunks.find(GetChunkCoordinate(above));
            const BlockType type = chunk == m_chunks.end()
                ? m_terrain->BlockAt(above)
                : chunk->second.blocks[GetBlockIndexHelper(chunk->second, above)].type;
            // Water lets the light through; anything else shades the cell.
            if (type != BlockType::Air && type != BlockType::Water)
            {
                return false;
            }
        }
        return true;
    }

    void VoxelWorld::SetBlockType(GridCell cell, BlockType type)
    {
        if (type != BlockType::Air && type != BlockType::Grass &&
            type != BlockType::Dirt && type != BlockType::Stone &&
            type != BlockType::Sand && type != BlockType::Water)
        {
            throw std::invalid_argument("Block has an unsupported type.");
        }
        if (cell.x < 0 || cell.x >= m_gridWidth ||
            cell.y < 0 || cell.y >= m_gridHeight ||
            cell.z < 0 || cell.z >= m_gridDepth)
        {
            throw std::out_of_range("Block cell is outside the grid.");
        }
        EnsureChunkGenerated(GetChunkCoordinate(cell));
        const ChunkCoordinate changedChunk = GetChunkCoordinate(cell);
        WorldChunk& chunk = m_chunks.at(changedChunk);
        Block& changed = chunk.blocks[GetBlockIndexHelper(chunk, cell)];
        changed.type = type;
        changed.material = kUnspecifiedMaterial; // the old material no longer applies
        ++m_terrainRevision;

        const int localX = cell.x % kChunkSize;
        const int localZ = cell.z % kChunkSize;
        const int lowerMeshChunkY = cell.y > 0
            ? (cell.y - 1) / kChunkSize
            : changedChunk.y;
        const int upperMeshChunkY = cell.y + 1 < m_gridHeight
            ? (cell.y + 1) / kChunkSize
            : changedChunk.y;

        for (auto& [coordinate, affectedChunk] : m_chunks)
        {
            const bool affectedByLighting =
                coordinate.x == changedChunk.x &&
                coordinate.z == changedChunk.z &&
                coordinate.y <= changedChunk.y;
            const bool affectedByUpperSurface =
                coordinate.x == changedChunk.x &&
                coordinate.z == changedChunk.z &&
                coordinate.y == upperMeshChunkY;
            const bool affectedByNeighborGeometry =
                ((localX == 0 && coordinate.x == changedChunk.x - 1 &&
                  coordinate.z == changedChunk.z) ||
                 (localX == kChunkSize - 1 && coordinate.x == changedChunk.x + 1 &&
                  coordinate.z == changedChunk.z) ||
                 (localZ == 0 && coordinate.x == changedChunk.x &&
                  coordinate.z == changedChunk.z - 1) ||
                 (localZ == kChunkSize - 1 && coordinate.x == changedChunk.x &&
                  coordinate.z == changedChunk.z + 1)) &&
                (coordinate.y == changedChunk.y ||
                 (cell.y > 0 && coordinate.y == lowerMeshChunkY));

            if (affectedByLighting || affectedByUpperSurface || affectedByNeighborGeometry)
            {
                ++affectedChunk.terrainRevision;
            }
        }
    }

    std::uint64_t VoxelWorld::GetTerrainRevision() const
    {
        return m_terrainRevision;
    }
}
