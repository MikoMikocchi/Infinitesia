#pragma once

#include <functional>
#include <optional>
#include <vector>

#include "world/Grid.h"

namespace Infinitesia
{
class GridPathfinder
{
public:
	using CellPassability = std::function<bool(GridCell)>;

	std::optional<std::vector<GridCell>> FindPath(GridCell start, GridCell goal, const Grid &grid,
	                                              const CellPassability &isCellPassable) const;
};
} // namespace Infinitesia
