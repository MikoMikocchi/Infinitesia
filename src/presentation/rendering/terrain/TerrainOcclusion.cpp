#include "presentation/rendering/terrain/TerrainOcclusion.h"

#include <algorithm>
#include <iterator>

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace
{
bool IsSolidCube(const BlockRegistry &registry, BlockState state)
{
	return registry.Occupied(state) && !registry.LetsLightThrough(state) && registry.FaceMask(state.shape) == kFaceAll;
}
} // namespace

const TerrainOcclusion::ColumnTops &TerrainOcclusion::GetColumn(const VoxelWorld &world, ColumnKey key, int topLevel)
{
	const auto cached = m_columns.find(key);
	if (cached != m_columns.end())
	{
		return cached->second.tops;
	}

	ColumnTops tops;
	tops.fill(-1);
	const BlockRegistry &registry = world.Registry();
	const auto &chunks = world.GetChunks();
	const int topChunkY = topLevel / kChunkSize;
	// Cells still looking for their solid block, from the top chunk down.
	int unresolved = kChunkSize * kChunkSize;
	for (int chunkY = topChunkY; chunkY >= 0 && unresolved > 0; --chunkY)
	{
		const auto found = chunks.find(ChunkCoordinate{key.first, chunkY, key.second});
		if (found == chunks.end())
		{
			break;
		}
		const WorldChunk &chunk = found->second;
		const int firstLevel = chunkY * kChunkSize;
		const int startLocalY = std::min(topLevel - firstLevel, chunk.height - 1);
		for (int localZ = 0; localZ < chunk.depth; ++localZ)
		{
			for (int localX = 0; localX < chunk.width; ++localX)
			{
				int &top = tops[static_cast<std::size_t>(localZ * kChunkSize + localX)];
				if (top >= 0)
				{
					continue;
				}
				for (int localY = startLocalY; localY >= 0; --localY)
				{
					const std::size_t index =
					    (static_cast<std::size_t>(localZ) * static_cast<std::size_t>(chunk.height) +
						 static_cast<std::size_t>(localY)) *
					        static_cast<std::size_t>(chunk.width) +
					    static_cast<std::size_t>(localX);
					if (IsSolidCube(registry, chunk.blocks[index].state))
					{
						top = firstLevel + localY;
						--unresolved;
						break;
					}
				}
			}
		}
	}
	CachedColumn column;
	column.tops = tops;
	column.complete = unresolved == 0;
	return m_columns.emplace(key, column).first->second.tops;
}

bool TerrainOcclusion::IsHidden(const VoxelWorld &world, const WorldChunk &chunk, int topLevel)
{
	const std::size_t chunkCount = world.GetChunks().size();
	if (m_world != &world || m_topLevel != topLevel || m_terrainRevision != world.GetTerrainRevision())
	{
		m_columns.clear();
		m_world = &world;
		m_topLevel = topLevel;
		m_terrainRevision = world.GetTerrainRevision();
		m_chunkCount = chunkCount;
	}
	else if (m_chunkCount != chunkCount)
	{
		// A generated chunk can only complete columns that stopped at a missing
		// chunk; the columns that already found their solid block cannot change.
		for (auto column = m_columns.begin(); column != m_columns.end();)
		{
			column = column->second.complete ? std::next(column) : m_columns.erase(column);
		}
		m_chunkCount = chunkCount;
	}

	const int chunkTop = chunk.coordinate.y * kChunkSize + chunk.height - 1;
	if (topLevel <= chunkTop)
	{
		return false;
	}

	const int firstX = chunk.coordinate.x * kChunkSize - 1;
	const int firstZ = chunk.coordinate.z * kChunkSize - 1;
	for (int z = firstZ; z <= firstZ + chunk.depth + 1; ++z)
	{
		for (int x = firstX; x <= firstX + chunk.width + 1; ++x)
		{
			if (x < 0 || z < 0)
			{
				return false; // the edge of the world shows its side
			}
			const ColumnTops &column = GetColumn(world, {x / kChunkSize, z / kChunkSize}, topLevel);
			if (column[static_cast<std::size_t>((z % kChunkSize) * kChunkSize + x % kChunkSize)] <= chunkTop)
			{
				return false;
			}
		}
	}
	return true;
}
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
