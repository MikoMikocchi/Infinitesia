#pragma once

#include "world/Block.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Infinitesia
{
    inline constexpr int kChunkSize = 16;

    struct ChunkCoordinate
    {
        int x;
        int y;
        int z;

        bool operator==(const ChunkCoordinate&) const = default;
        bool operator<(const ChunkCoordinate& other) const
        {
            if (x != other.x)
            {
                return x < other.x;
            }
            if (y != other.y)
            {
                return y < other.y;
            }
            return z < other.z;
        }
    };

    struct WorldChunk
    {
        ChunkCoordinate coordinate;
        int width;
        int height;
        int depth;
        std::uint64_t terrainRevision = 0;
        std::vector<Block> blocks;
    };
}
