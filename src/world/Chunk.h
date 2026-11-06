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

// Hashes a chunk coordinate for the unordered chunk table: GetBlock and the mesh
// builder ask for one neighbour after another, so a tree is the wrong shape.
struct ChunkCoordinateHash
{
	std::size_t operator()(const ChunkCoordinate &coordinate) const noexcept
	{
		std::uint64_t hash = 1469598103934665603ull; // FNV-1a
		const auto mix = [&hash](std::uint32_t value) {
			hash ^= value;
			hash *= 1099511628211ull;
		};
		mix(static_cast<std::uint32_t>(coordinate.x));
		mix(static_cast<std::uint32_t>(coordinate.y));
		mix(static_cast<std::uint32_t>(coordinate.z));
		return static_cast<std::size_t>(hash);
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
