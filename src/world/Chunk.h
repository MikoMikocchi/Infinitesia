#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "world/Block.h"

namespace Infinitesia
{
inline constexpr int kChunkSize = 16;

struct ChunkCoordinate
{
	int x;
	int y;
	int z;

	bool operator==(const ChunkCoordinate &) const = default;
	bool operator<(const ChunkCoordinate &other) const
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
	// A player or simulation changed a block here. Unmodified chunks can be
	// dropped when they leave the view and regenerated from the terrain source.
	bool modified = false;
	std::vector<Block> blocks;
};
} // namespace Infinitesia
