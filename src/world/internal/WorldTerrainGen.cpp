#include "world/internal/WorldTerrainGen.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
    namespace WorldInternal
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

        std::size_t GetBlockIndex(const WorldChunk& chunk, GridCell cell)
        {
            const std::size_t localX = static_cast<std::size_t>(cell.x % kChunkSize);
            const std::size_t localY = static_cast<std::size_t>(cell.y % kChunkSize);
            const std::size_t localZ = static_cast<std::size_t>(cell.z % kChunkSize);
            return (localZ * static_cast<std::size_t>(chunk.height) + localY) *
                       static_cast<std::size_t>(chunk.width) +
                   localX;
        }

        BlockType GenerateBlockType(int gridHeight, std::uint32_t seed, GridCell cell)
        {
            if (cell.y < gridHeight - 1)
            {
                return cell.y >= gridHeight - 4 ? BlockType::Dirt : BlockType::Stone;
            }

            const float noise = TerrainNoise(cell.x, cell.z, seed);
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
}
