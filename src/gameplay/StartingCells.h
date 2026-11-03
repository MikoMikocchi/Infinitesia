#pragma once

#include "world/Grid.h"
#include "world/TerrainSource.h"

#include <optional>

namespace Infinitesia
{
    // Where a new settlement puts its first worker and the tree he is sent to chop.
    struct StartingCells
    {
        GridCell worker;
        // Empty when no tree stands within reach of the worker.
        std::optional<GridCell> tree;
    };

    // Level of the topmost solid block (not air, not water) of a column, found by looking
    // down the column; -1 when there is none. Columns outside the grid have none.
    int SurfaceLevelOf(const Grid& grid, const TerrainSource& terrain, int x, int z);

    // The worker stands on the ground of the middle column. The tree is the nearest cell
    // (3 to 16 columns away, searched ring by ring) on the worker's level that he can walk
    // to. Movement is still by level (a cell is walkable where there is a block on the
    // worker's level), so every cell of the way there, east or west first and then south
    // or north, must have ground at that level or higher.
    StartingCells FindStartingCells(const Grid& grid, const TerrainSource& terrain);
}
