#include "world/VoxelWorld.h"

#include <stdexcept>
#include <utility>

namespace Infinitesia
{
VoxelWorld::VoxelWorld(const Grid &grid, std::uint32_t seed)
    : VoxelWorld(grid, std::make_shared<NoiseTerrainSource>(grid.GetHeight(), seed))
{
}

VoxelWorld::VoxelWorld(const Grid &grid, std::shared_ptr<const TerrainSource> terrain)
    : m_gridWidth(grid.GetWidth()), m_gridHeight(grid.GetHeight()), m_gridDepth(grid.GetDepth()),
      m_terrain(std::move(terrain))
{
	if (m_terrain == nullptr)
	{
		throw std::invalid_argument("World needs a terrain source.");
	}
}

void VoxelWorld::EnsureChunkContaining(GridCell cell)
{
	if (cell.x < 0 || cell.x >= m_gridWidth || cell.y < 0 || cell.y >= m_gridHeight || cell.z < 0 ||
	    cell.z >= m_gridDepth)
	{
		throw std::out_of_range("Cell is outside the grid.");
	}
	EnsureChunkGenerated(GetChunkCoordinate(cell));
}
} // namespace Infinitesia
