#pragma once

#include "world/Grid.h"
#include "world/TerrainSource.h"

namespace Infinitesia
{
// Where a new settlement puts its first worker.
struct StartingCells
{
	GridCell worker;
};

// Level of the topmost solid block (not air, not water) of a column, found by
// looking down the column; -1 when there is none. Columns outside the grid have
// none.
int SurfaceLevelOf(const Grid &grid, const TerrainSource &terrain, int x, int z);

// The worker stands on the ground of the middle column.
StartingCells FindStartingCells(const Grid &grid, const TerrainSource &terrain);
} // namespace Infinitesia
