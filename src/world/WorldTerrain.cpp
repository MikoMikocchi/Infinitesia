#include "world/World.h"

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
            return (cellCount + kChunkSize - 1) / kChunkSize;
        }

        std::uint32_t HashCoordinates(int x, int z, std::uint32_t seed)
        {
            std::uint32_t value = seed;
            value ^= static_cast<std::uint32_t>(x) * 0x9E3779B9u;
            value = (value ^ (value >> 16)) * 0x85EBCA6Bu;
            value ^= static_cast<std::uint32_t>(z) * 0xC2B2AE35u;
            value = (value ^ (value >> 13)) * 0x27D4EB2Fu;
            return value ^ (value >> 16);
        }

        float SmoothNoise(float value)
        {
            return value * value * (3.0f - 2.0f * value);
        }

        float LatticeValue(int x, int z, std::uint32_t seed)
        {
            constexpr float kInverseMaximum = 1.0f / 4294967295.0f;
            return static_cast<float>(HashCoordinates(x, z, seed)) * kInverseMaximum;
        }

        float TerrainNoise(int x, int z, std::uint32_t seed)
        {
            constexpr int kNoiseScale = 12;
            const int latticeX = x / kNoiseScale;
            const int latticeZ = z / kNoiseScale;
            const float blendX = SmoothNoise(
                static_cast<float>(x % kNoiseScale) / static_cast<float>(kNoiseScale));
            const float blendZ = SmoothNoise(
                static_cast<float>(z % kNoiseScale) / static_cast<float>(kNoiseScale));
            const float top = LatticeValue(latticeX, latticeZ, seed) * (1.0f - blendX) +
                              LatticeValue(latticeX + 1, latticeZ, seed) * blendX;
            const float bottom = LatticeValue(latticeX, latticeZ + 1, seed) * (1.0f - blendX) +
                                 LatticeValue(latticeX + 1, latticeZ + 1, seed) * blendX;
            return top * (1.0f - blendZ) + bottom * blendZ;
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
        return &chunk->second.blocks[GetBlockIndex(chunk->second, cell)];
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
                ? GenerateBlockType(above)
                : chunk->second.blocks[GetBlockIndex(chunk->second, above)].type;
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
        chunk.blocks[GetBlockIndex(chunk, cell)].type = type;
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
                    chunk.blocks.push_back({ cell, GenerateBlockType(cell) });
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
        const std::size_t localX = static_cast<std::size_t>(cell.x % kChunkSize);
        const std::size_t localY = static_cast<std::size_t>(cell.y % kChunkSize);
        const std::size_t localZ = static_cast<std::size_t>(cell.z % kChunkSize);
        return (localZ * static_cast<std::size_t>(chunk.height) + localY) *
                   static_cast<std::size_t>(chunk.width) +
               localX;
    }

    BlockType World::GenerateBlockType(GridCell cell) const
    {
        if (cell.y < m_gridHeight - 1)
        {
            return cell.y >= m_gridHeight - 4 ? BlockType::Dirt : BlockType::Stone;
        }

        const float noise = TerrainNoise(cell.x, cell.z, m_seed);
        if (noise < 0.25f)
        {
            return BlockType::Dirt;
        }
        if (noise > 0.78f)
        {
            return BlockType::Stone;
        }
        return BlockType::Grass;
    }
}
