#include "world/World.h"
#include "world/internal/WorldTerrainGen.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        int GetChunkCount(int cellCount)
        {
            return WorldInternal::GetChunkCount(cellCount);
        }

        std::size_t GetBlockIndexHelper(const WorldChunk& chunk, GridCell cell)
        {
            return WorldInternal::GetBlockIndex(chunk, cell);
        }
    }

    void World::RequestChunksAround(GridCell center, int horizontalRadius, int verticalRadius)
    {
        if (horizontalRadius < 0 || verticalRadius < 0)
        {
            throw std::invalid_argument("Chunk generation radii cannot be negative.");
        }
        if (center.x < 0 || center.x >= m_gridWidth ||
            center.y < 0 || center.y >= m_gridHeight ||
            center.z < 0 || center.z >= m_gridDepth)
        {
            throw std::out_of_range("Chunk generation center is outside the world.");
        }

        const ChunkCoordinate centerChunk = GetChunkCoordinate(center);
        const int maxChunkX = GetChunkCount(m_gridWidth) - 1;
        const int maxChunkY = GetChunkCount(m_gridHeight) - 1;
        const int maxChunkZ = GetChunkCount(m_gridDepth) - 1;
        std::vector<ChunkCoordinate> requestedChunks;

        for (int chunkY = std::max(0, centerChunk.y - verticalRadius);
             chunkY <= std::min(maxChunkY, centerChunk.y + verticalRadius);
             ++chunkY)
        {
            for (int chunkZ = std::max(0, centerChunk.z - horizontalRadius);
                 chunkZ <= std::min(maxChunkZ, centerChunk.z + horizontalRadius);
                 ++chunkZ)
            {
                for (int chunkX = std::max(0, centerChunk.x - horizontalRadius);
                     chunkX <= std::min(maxChunkX, centerChunk.x + horizontalRadius);
                     ++chunkX)
                {
                    const ChunkCoordinate coordinate{ chunkX, chunkY, chunkZ };
                    if (m_chunks.find(coordinate) == m_chunks.end())
                    {
                        requestedChunks.push_back(coordinate);
                    }
                }
            }
        }

        std::sort(
            requestedChunks.begin(),
            requestedChunks.end(),
            [centerChunk](ChunkCoordinate left, ChunkCoordinate right)
            {
                const int leftDistance =
                    std::abs(left.x - centerChunk.x) + std::abs(left.y - centerChunk.y) +
                    std::abs(left.z - centerChunk.z);
                const int rightDistance =
                    std::abs(right.x - centerChunk.x) + std::abs(right.y - centerChunk.y) +
                    std::abs(right.z - centerChunk.z);
                if (leftDistance != rightDistance)
                {
                    return leftDistance < rightDistance;
                }
                if (left.x != right.x)
                {
                    return left.x < right.x;
                }
                if (left.y != right.y)
                {
                    return left.y < right.y;
                }
                return left.z < right.z;
            });
        m_pendingChunks = std::move(requestedChunks);
    }

    std::size_t World::GeneratePendingChunks(std::size_t maximumChunks)
    {
        std::size_t generatedChunks = 0;
        while (generatedChunks < maximumChunks && !m_pendingChunks.empty())
        {
            const ChunkCoordinate coordinate = m_pendingChunks.front();
            m_pendingChunks.erase(m_pendingChunks.begin());
            EnsureChunkGenerated(coordinate);
            ++generatedChunks;
        }
        return generatedChunks;
    }

    const Block* World::GetBlock(GridCell cell) const
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

    bool World::HasSkyAccess(GridCell cell) const
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
                ? WorldInternal::GenerateBlockType(m_gridHeight, m_seed, above)
                : chunk->second.blocks[GetBlockIndexHelper(chunk->second, above)].type;
            if (type != BlockType::Air)
            {
                return false;
            }
        }
        return true;
    }

    void World::SetBlockType(GridCell cell, BlockType type)
    {
        if (type != BlockType::Air && type != BlockType::Grass &&
            type != BlockType::Dirt && type != BlockType::Stone)
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
        WorldChunk& chunk = m_chunks.at(GetChunkCoordinate(cell));
        chunk.blocks[GetBlockIndexHelper(chunk, cell)].type = type;
        ++chunk.terrainRevision;
        ++m_terrainRevision;
    }

    std::uint64_t World::GetTerrainRevision() const
    {
        return m_terrainRevision;
    }

    const std::map<ChunkCoordinate, WorldChunk>& World::GetChunks() const
    {
        return m_chunks;
    }

    std::size_t World::GetLoadedChunkCount() const
    {
        return m_chunks.size();
    }

    std::size_t World::GetTotalChunkCount() const
    {
        return static_cast<std::size_t>(GetChunkCount(m_gridWidth)) *
               static_cast<std::size_t>(GetChunkCount(m_gridHeight)) *
               static_cast<std::size_t>(GetChunkCount(m_gridDepth));
    }

    void World::EnsureChunkGenerated(ChunkCoordinate coordinate)
    {
        if (m_chunks.find(coordinate) != m_chunks.end())
        {
            return;
        }

        const int startX = coordinate.x * kChunkSize;
        const int startY = coordinate.y * kChunkSize;
        const int startZ = coordinate.z * kChunkSize;
        const int chunkWidth = std::min(kChunkSize, m_gridWidth - startX);
        const int chunkHeight = std::min(kChunkSize, m_gridHeight - startY);
        const int chunkDepth = std::min(kChunkSize, m_gridDepth - startZ);
        WorldChunk chunk{
            coordinate,
            chunkWidth,
            chunkHeight,
            chunkDepth,
            0,
            {}
        };
        chunk.blocks.reserve(
            static_cast<std::size_t>(chunkWidth) *
            static_cast<std::size_t>(chunkHeight) *
            static_cast<std::size_t>(chunkDepth));

        for (int localZ = 0; localZ < chunkDepth; ++localZ)
        {
            for (int localY = 0; localY < chunkHeight; ++localY)
            {
                for (int localX = 0; localX < chunkWidth; ++localX)
                {
                    const GridCell cell{
                        startX + localX,
                        startY + localY,
                        startZ + localZ
                    };
                    chunk.blocks.push_back({ cell, WorldInternal::GenerateBlockType(m_gridHeight, m_seed, cell) });
                }
            }
        }

        m_chunks.emplace(coordinate, std::move(chunk));
    }

    ChunkCoordinate World::GetChunkCoordinate(GridCell cell) const
    {
        return { cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize };
    }

    std::size_t World::GetBlockIndex(const WorldChunk& chunk, GridCell cell) const
    {
        return WorldInternal::GetBlockIndex(chunk, cell);
    }

    BlockType World::GenerateBlockType(GridCell cell) const
    {
        return WorldInternal::GenerateBlockType(m_gridHeight, m_seed, cell);
    }
}
