#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "world/VoxelWorld.h"
#include "world/internal/WorldTerrainGen.h"

namespace Infinitesia
{
namespace
{
int GetChunkCount(int cellCount)
{
	return WorldInternal::GetChunkCount(cellCount);
}
} // namespace

void VoxelWorld::RequestChunksAround(GridCell center, int horizontalRadius, int verticalRadius)
{
	if (horizontalRadius < 0 || verticalRadius < 0)
	{
		throw std::invalid_argument("Chunk generation radii cannot be negative.");
	}
	if (center.x < 0 || center.x >= m_gridWidth || center.y < 0 || center.y >= m_gridHeight || center.z < 0 ||
	    center.z >= m_gridDepth)
	{
		throw std::out_of_range("Chunk generation center is outside the world.");
	}

	const ChunkCoordinate centerChunk = GetChunkCoordinate(center);
	const int maxChunkX = GetChunkCount(m_gridWidth) - 1;
	const int maxChunkY = GetChunkCount(m_gridHeight) - 1;
	const int maxChunkZ = GetChunkCount(m_gridDepth) - 1;
	std::vector<ChunkCoordinate> requestedChunks;

	for (int chunkY = std::max(0, centerChunk.y - verticalRadius);
	     chunkY <= std::min(maxChunkY, centerChunk.y + verticalRadius); ++chunkY)
	{
		for (int chunkZ = std::max(0, centerChunk.z - horizontalRadius);
		     chunkZ <= std::min(maxChunkZ, centerChunk.z + horizontalRadius); ++chunkZ)
		{
			for (int chunkX = std::max(0, centerChunk.x - horizontalRadius);
			     chunkX <= std::min(maxChunkX, centerChunk.x + horizontalRadius); ++chunkX)
			{
				const ChunkCoordinate coordinate{chunkX, chunkY, chunkZ};
				if (m_chunks.find(coordinate) == m_chunks.end())
				{
					requestedChunks.push_back(coordinate);
				}
			}
		}
	}

	std::sort(requestedChunks.begin(), requestedChunks.end(),
	          [centerChunk](ChunkCoordinate left, ChunkCoordinate right) {
		          const int leftDistance = std::abs(left.x - centerChunk.x) + std::abs(left.y - centerChunk.y) +
				                           std::abs(left.z - centerChunk.z);
		          const int rightDistance = std::abs(right.x - centerChunk.x) + std::abs(right.y - centerChunk.y) +
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

std::size_t VoxelWorld::GeneratePendingChunks(std::size_t maximumChunks)
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

void VoxelWorld::UnloadChunksOutside(GridCell center, int horizontalRadius, int verticalRadius)
{
	if (horizontalRadius < 0 || verticalRadius < 0)
	{
		throw std::invalid_argument("Chunk unload radii cannot be negative.");
	}
	if (!Contains(center))
	{
		throw std::out_of_range("Chunk unload center is outside the world.");
	}
	const ChunkCoordinate centerChunk = GetChunkCoordinate(center);
	const std::map<ChunkCoordinate, FluidChunk> &fluidChunks = m_fluids.Chunks();

	for (auto chunk = m_chunks.begin(); chunk != m_chunks.end();)
	{
		const ChunkCoordinate &coordinate = chunk->first;
		const bool inside = std::abs(coordinate.x - centerChunk.x) <= horizontalRadius &&
		                    std::abs(coordinate.z - centerChunk.z) <= horizontalRadius &&
		                    std::abs(coordinate.y - centerChunk.y) <= verticalRadius;
		if (inside || chunk->second.modified || fluidChunks.find(coordinate) != fluidChunks.end())
		{
			++chunk;
			continue;
		}
		chunk = m_chunks.erase(chunk);
	}

	m_pendingChunks.erase(std::remove_if(m_pendingChunks.begin(), m_pendingChunks.end(),
	                                     [&](const ChunkCoordinate &coordinate) {
		                                     return std::abs(coordinate.x - centerChunk.x) > horizontalRadius ||
											        std::abs(coordinate.z - centerChunk.z) > horizontalRadius ||
											        std::abs(coordinate.y - centerChunk.y) > verticalRadius;
	                                     }),
	                      m_pendingChunks.end());
}

const std::map<ChunkCoordinate, WorldChunk> &VoxelWorld::GetChunks() const
{
	return m_chunks;
}

std::size_t VoxelWorld::GetLoadedChunkCount() const
{
	return m_chunks.size();
}

std::size_t VoxelWorld::GetTotalChunkCount() const
{
	return static_cast<std::size_t>(GetChunkCount(m_gridWidth)) *
	       static_cast<std::size_t>(GetChunkCount(m_gridHeight)) * static_cast<std::size_t>(GetChunkCount(m_gridDepth));
}

void VoxelWorld::EnsureChunkGenerated(ChunkCoordinate coordinate)
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
	WorldChunk chunk{coordinate, chunkWidth, chunkHeight, chunkDepth, 0, false, {}};
	chunk.blocks.reserve(static_cast<std::size_t>(chunkWidth) * static_cast<std::size_t>(chunkHeight) *
	                     static_cast<std::size_t>(chunkDepth));

	for (int localZ = 0; localZ < chunkDepth; ++localZ)
	{
		for (int localY = 0; localY < chunkHeight; ++localY)
		{
			for (int localX = 0; localX < chunkWidth; ++localX)
			{
				const GridCell cell{startX + localX, startY + localY, startZ + localZ};
				chunk.blocks.push_back({cell, m_terrain->StateAt(cell)});
			}
		}
	}

	const auto inserted = m_chunks.emplace(coordinate, std::move(chunk));
	m_fluids.InitializeChunk(inserted.first->second, *m_terrain);

	// The meshes of the loaded neighbours were built with this chunk counted as
	// open air; their border faces and ambient occlusion are stale now.
	for (const ChunkCoordinate &next : {ChunkCoordinate{coordinate.x - 1, coordinate.y, coordinate.z},
	                                    ChunkCoordinate{coordinate.x + 1, coordinate.y, coordinate.z},
	                                    ChunkCoordinate{coordinate.x, coordinate.y - 1, coordinate.z},
	                                    ChunkCoordinate{coordinate.x, coordinate.y + 1, coordinate.z},
	                                    ChunkCoordinate{coordinate.x, coordinate.y, coordinate.z - 1},
	                                    ChunkCoordinate{coordinate.x, coordinate.y, coordinate.z + 1}})
	{
		const auto neighbor = m_chunks.find(next);
		if (neighbor != m_chunks.end())
		{
			++neighbor->second.terrainRevision;
		}
	}
}

ChunkCoordinate VoxelWorld::GetChunkCoordinate(GridCell cell) const
{
	return {cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize};
}

std::size_t VoxelWorld::GetBlockIndex(const WorldChunk &chunk, GridCell cell) const
{
	return WorldInternal::GetBlockIndex(chunk, cell);
}
} // namespace Infinitesia
