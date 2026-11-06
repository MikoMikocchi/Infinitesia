#include <algorithm>
#include <set>
#include <stdexcept>

#include "world/VoxelWorld.h"
#include "world/internal/WorldTerrainGen.h"

namespace Infinitesia
{
namespace
{
std::size_t GetBlockIndexHelper(const WorldChunk &chunk, GridCell cell)
{
	return WorldInternal::GetBlockIndex(chunk, cell);
}
} // namespace

const Block *VoxelWorld::GetBlock(GridCell cell) const
{
	if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 || cell.y >= m_gridHeight || cell.z < 0 ||
	    cell.z >= m_gridDepth)
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

BlockState VoxelWorld::GroundStateAt(GridCell cell) const
{
	if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 || cell.y >= m_gridHeight || cell.z < 0 ||
	    cell.z >= m_gridDepth)
	{
		return {};
	}
	return m_terrain->GroundStateAt(cell);
}

bool VoxelWorld::HasSkyAccess(GridCell cell) const
{
	if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 || cell.y >= m_gridHeight || cell.z < 0 ||
	    cell.z >= m_gridDepth)
	{
		return false;
	}

	const BlockRegistry &registry = m_terrain->Registry();
	int y = cell.y + 1;
	// The cells of one chunk are looked up once and then indexed: an unbroken
	// column used to cost a chunk lookup per metre, which matters for the deep
	// chunks under the sea.
	while (y < m_gridHeight)
	{
		const ChunkCoordinate coordinate = GetChunkCoordinate(GridCell{cell.x, y, cell.z});
		const auto chunk = m_chunks.find(coordinate);
		if (chunk == m_chunks.end())
		{
			// Not generated: there is no storage to index, so ask the source. The
			// loop still advances a whole chunk at a time.
			const int end = std::min(m_gridHeight, (coordinate.y + 1) * kChunkSize);
			for (; y < end; ++y)
			{
				if (!registry.LetsLightThrough(m_terrain->StateAt(GridCell{cell.x, y, cell.z})))
				{
					return false;
				}
			}
			continue;
		}

		const WorldChunk &loaded = chunk->second;
		const int end = std::min(m_gridHeight, coordinate.y * kChunkSize + loaded.height);
		for (; y < end; ++y)
		{
			const GridCell above{cell.x, y, cell.z};
			// Air and water let the light through; anything that stops it shades the
			// cell.
			if (!registry.LetsLightThrough(loaded.blocks[GetBlockIndexHelper(loaded, above)].state))
			{
				return false;
			}
		}
	}
	return true;
}

void VoxelWorld::SetBlock(GridCell cell, BlockState state)
{
	if (m_terrain->Registry().GetMatter(state.matter) == nullptr ||
	    m_terrain->Registry().GetShape(state.shape) == nullptr)
	{
		throw std::invalid_argument("The block registry does not know the material or shape of the block.");
	}
	if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 || cell.y >= m_gridHeight || cell.z < 0 ||
	    cell.z >= m_gridDepth)
	{
		throw std::out_of_range("Block cell is outside the grid.");
	}
	EnsureChunkGenerated(GetChunkCoordinate(cell));
	const ChunkCoordinate changedChunk = GetChunkCoordinate(cell);
	WorldChunk &chunk = m_chunks.at(changedChunk);
	chunk.modified = true;
	Block &changed = chunk.blocks[GetBlockIndexHelper(chunk, cell)];
	changed.state = state;
	++m_terrainRevision;
	// Keep the separate fluid layer in step: a dug cell lets water in, a placed
	// block pushes it out.
	m_fluids.OnBlockChanged(cell, *this);

	// A mesh depends on the blocks one cell around its own (hidden faces and the
	// ambient occlusion of their corners, diagonals included), so every chunk
	// holding a cell of the 3x3x3 block around the changed one is stale.
	std::set<ChunkCoordinate> meshNeighbors;
	for (int dy = -1; dy <= 1; ++dy)
	{
		for (int dz = -1; dz <= 1; ++dz)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				const GridCell around{cell.x + dx, cell.y + dy, cell.z + dz};
				if (Contains(around))
				{
					meshNeighbors.insert(GetChunkCoordinate(around));
				}
			}
		}
	}

	// Light from the sky changes for the chunks below in the same column; only
	// that column has to be walked, not every loaded chunk.
	for (int y = 0; y <= changedChunk.y; ++y)
	{
		const auto below = m_chunks.find(ChunkCoordinate{changedChunk.x, y, changedChunk.z});
		if (below != m_chunks.end())
		{
			++below->second.terrainRevision;
		}
	}
	for (const ChunkCoordinate &coordinate : meshNeighbors)
	{
		const auto neighbor = m_chunks.find(coordinate);
		if (neighbor != m_chunks.end())
		{
			++neighbor->second.terrainRevision;
		}
	}
}

const FluidCell *VoxelWorld::GetFluid(GridCell cell) const
{
	return m_fluids.At(cell);
}

void VoxelWorld::StepFluids()
{
	m_fluids.Step(*this);
}

std::uint64_t VoxelWorld::GetTerrainRevision() const
{
	return m_terrainRevision;
}
} // namespace Infinitesia
