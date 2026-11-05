#include "gameplay/work/TreeFelling.h"

#include <algorithm>
#include <climits>
#include <deque>
#include <set>

namespace Infinitesia
{
namespace
{
// Foliage is looked for this far beyond the felled cells for wood that could
// still hold it.
constexpr int kSupportMargin = kLeafSupportSteps;

bool HasTag(const VoxelWorld &terrain, GridCell cell, Tag tag)
{
	const Block *block = terrain.GetBlock(cell);
	return block != nullptr && terrain.Registry().Has(block->state.matter, tag);
}

struct CellLess
{
	bool operator()(const GridCell &a, const GridCell &b) const
	{
		if (a.y != b.y)
			return a.y < b.y;
		if (a.z != b.z)
			return a.z < b.z;
		return a.x < b.x;
	}
};
} // namespace

bool IsTreeWood(const VoxelWorld &terrain, GridCell cell)
{
	return HasTag(terrain, cell, Tag::TreeWood);
}

bool IsTreeLeaves(const VoxelWorld &terrain, GridCell cell)
{
	return HasTag(terrain, cell, Tag::TreeLeaves);
}

std::vector<GridCell> FindTreeWood(const VoxelWorld &terrain, GridCell start)
{
	std::vector<GridCell> wood;
	if (!IsTreeWood(terrain, start))
		return wood;

	std::set<GridCell, CellLess> seen{start};
	std::deque<GridCell> queue{start};
	while (!queue.empty() && wood.size() < kMaxTreeWoodCells)
	{
		const GridCell cell = queue.front();
		queue.pop_front();
		wood.push_back(cell);
		for (int dz = -1; dz <= 1; ++dz)
		{
			for (int dy = -1; dy <= 1; ++dy)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					const GridCell next{cell.x + dx, cell.y + dy, cell.z + dz};
					if (IsTreeWood(terrain, next) && seen.insert(next).second)
						queue.push_back(next);
				}
			}
		}
	}
	return wood;
}

FelledTree FellTree(VoxelWorld &terrain, GridCell start)
{
	FelledTree felled;
	felled.wood = FindTreeWood(terrain, start);
	if (felled.wood.empty())
		return felled;

	int minX = INT_MAX, minY = INT_MAX, minZ = INT_MAX;
	int maxX = INT_MIN, maxY = INT_MIN, maxZ = INT_MIN;
	for (const GridCell &cell : felled.wood)
	{
		const Block *block = terrain.GetBlock(cell);
		const MatterDef *material = terrain.Registry().GetMatter(block->state.matter);
		felled.woodKilograms += material != nullptr ? material->density : 0.0;
		terrain.SetBlock(cell, terrain.GroundStateAt(cell));
		minX = std::min(minX, cell.x);
		maxX = std::max(maxX, cell.x);
		minY = std::min(minY, cell.y);
		maxY = std::max(maxY, cell.y);
		minZ = std::min(minZ, cell.z);
		maxZ = std::max(maxZ, cell.z);
	}

	// The box in which wood can still hold foliage that the felled wood held, and
	// the box of that foliage.
	const GridCell low{minX - 2 * kSupportMargin, minY - 2 * kSupportMargin, minZ - 2 * kSupportMargin};
	const GridCell high{maxX + 2 * kSupportMargin, maxY + 2 * kSupportMargin, maxZ + 2 * kSupportMargin};
	const auto inBox = [&](GridCell cell, int margin) {
		return cell.x >= minX - margin && cell.x <= maxX + margin && cell.y >= minY - margin &&
		       cell.y <= maxY + margin && cell.z >= minZ - margin && cell.z <= maxZ + margin;
	};

	// Foliage is held by wood within kLeafSupportSteps steps through foliage and
	// wood: search outwards from all the wood left in the box.
	std::set<GridCell, CellLess> held;
	std::deque<std::pair<GridCell, int>> queue;
	std::vector<GridCell> foliage;
	for (int z = low.z; z <= high.z; ++z)
	{
		for (int y = low.y; y <= high.y; ++y)
		{
			for (int x = low.x; x <= high.x; ++x)
			{
				const GridCell cell{x, y, z};
				if (!terrain.Contains(cell))
					continue;
				// Chunks are made on demand, and foliage in one nobody has touched is
				// still there to fall.
				if (terrain.GetBlock(cell) == nullptr)
					terrain.EnsureChunkContaining(cell);
				if (IsTreeWood(terrain, cell))
				{
					queue.push_back({cell, 0});
					held.insert(cell);
				}
				else if (IsTreeLeaves(terrain, cell) && inBox(cell, kSupportMargin))
				{
					foliage.push_back(cell);
				}
			}
		}
	}
	while (!queue.empty())
	{
		const auto [cell, steps] = queue.front();
		queue.pop_front();
		if (steps >= kLeafSupportSteps)
			continue;
		for (int dz = -1; dz <= 1; ++dz)
		{
			for (int dy = -1; dy <= 1; ++dy)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					const GridCell next{cell.x + dx, cell.y + dy, cell.z + dz};
					if (IsTreeLeaves(terrain, next) && held.insert(next).second)
						queue.push_back({next, steps + 1});
				}
			}
		}
	}
	for (const GridCell &cell : foliage)
	{
		if (held.contains(cell))
			continue;
		terrain.SetBlock(cell, terrain.GroundStateAt(cell));
		++felled.leavesFallen;
	}
	return felled;
}
} // namespace Infinitesia
