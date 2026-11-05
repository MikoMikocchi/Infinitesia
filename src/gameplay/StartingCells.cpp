#include "gameplay/StartingCells.h"

namespace Infinitesia
{
int SurfaceLevelOf(const Grid &grid, const TerrainSource &terrain, int x, int z)
{
	if (x < 0 || x >= grid.GetWidth() || z < 0 || z >= grid.GetDepth())
	{
		return -1;
	}
	const BlockRegistry &registry = terrain.Registry();
	for (int y = grid.GetHeight() - 1; y >= 0; --y)
	{
		const BlockState state = terrain.StateAt({x, y, z});
		// Ground to stand on: something is there and it is neither water nor a
		// plant.
		if (registry.Occupied(state) && !registry.Has(state.matter, Tag::Water) &&
		    !registry.Has(state.matter, Tag::Plant))
		{
			return y;
		}
	}
	return -1;
}

StartingCells FindStartingCells(const Grid &grid, const TerrainSource &terrain)
{
	const int centerX = grid.GetWidth() / 2;
	const int centerZ = grid.GetDepth() / 2;
	return {{centerX, SurfaceLevelOf(grid, terrain, centerX, centerZ), centerZ}};
}
} // namespace Infinitesia
