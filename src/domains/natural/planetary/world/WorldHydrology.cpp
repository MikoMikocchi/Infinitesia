#include "domains/natural/planetary/world/WorldHydrology.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <queue>
#include <utility>
#include <vector>

namespace Infinitesia
{
namespace
{
constexpr float kFillEpsilonMeters = 0.01f;
constexpr float kFlowEpsilonMeters = 1.0e-4f;
constexpr float kTileAreaKm2 = static_cast<float>((kRegionTileSizeMeters / 1000.0) * (kRegionTileSizeMeters / 1000.0));

struct FloodNode
{
	float level = 0.0f;
	int index = 0;
};

struct HigherFloodNode
{
	bool operator()(const FloodNode &first, const FloodNode &second) const noexcept
	{
		if (first.level != second.level)
		{
			return first.level > second.level;
		}
		return first.index > second.index;
	}
};

int NeighbourIndex(int i, int j, int deltaI, int deltaJ) noexcept
{
	const RegionTileId id{i + deltaI, j + deltaJ};
	return WorldMap::Contains(id) ? static_cast<int>(WorldMap::Index(id)) : -1;
}
} // namespace

void ComputeWorldHydrology(std::vector<RegionTile> &tiles)
{
	constexpr int side = kRegionTilesPerWorldSide;
	constexpr int half = kRegionTilesPerWorldHalfSide;
	const std::size_t count = static_cast<std::size_t>(side) * static_cast<std::size_t>(side);
	if (tiles.size() != count)
	{
		return;
	}

	const int deltaI[8] = {1, -1, 0, 0, 1, 1, -1, -1};
	const int deltaJ[8] = {0, 0, 1, -1, 1, -1, 1, -1};

	// Priority flood: every ocean tile and every map-edge tile is an outlet.
	// Depressions fill to their spill level, so the world has no closed pits
	// without a lake.
	std::vector<float> filled(count, std::numeric_limits<float>::infinity());
	std::vector<std::uint8_t> visited(count, 0);
	std::priority_queue<FloodNode, std::vector<FloodNode>, HigherFloodNode> open;

	const auto seed = [&](int index, float elevation) {
		filled[static_cast<std::size_t>(index)] = elevation;
		visited[static_cast<std::size_t>(index)] = 1;
		open.push({elevation, index});
	};

	for (int j = -half; j <= half; ++j)
	{
		for (int i = -half; i <= half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			const RegionTile &tile = tiles[static_cast<std::size_t>(index)];
			const bool edge = i == -half || i == half || j == -half || j == half;
			if (tile.IsOcean() || edge)
			{
				seed(index, tile.elevationMeters);
			}
		}
	}

	while (!open.empty())
	{
		const FloodNode node = open.top();
		open.pop();
		const int j = node.index / side - half;
		const int i = node.index % side - half;
		for (int direction = 0; direction < 8; ++direction)
		{
			const int neighbour = NeighbourIndex(i, j, deltaI[direction], deltaJ[direction]);
			if (neighbour < 0 || visited[static_cast<std::size_t>(neighbour)] != 0)
			{
				continue;
			}
			visited[static_cast<std::size_t>(neighbour)] = 1;
			const float elevation = tiles[static_cast<std::size_t>(neighbour)].elevationMeters;
			const float level = std::max(elevation, node.level + kFillEpsilonMeters);
			filled[static_cast<std::size_t>(neighbour)] = level;
			open.push({level, neighbour});
		}
	}

	// Flow to the lowest neighbour. A flat lake surface is allowed to drain to a
	// neighbour with a lower original elevation; the pair of strict decreases
	// (filled level or original elevation) keeps the graph acyclic.
	std::vector<int> downstream(count, -1);
	for (int j = -half; j <= half; ++j)
	{
		for (int i = -half; i <= half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			const RegionTile &tile = tiles[static_cast<std::size_t>(index)];
			if (tile.IsOcean())
			{
				continue;
			}
			const float level = filled[static_cast<std::size_t>(index)];
			int best = -1;
			float bestLevel = std::numeric_limits<float>::infinity();
			float bestElevation = std::numeric_limits<float>::infinity();
			for (int direction = 0; direction < 8; ++direction)
			{
				const int neighbour = NeighbourIndex(i, j, deltaI[direction], deltaJ[direction]);
				if (neighbour < 0)
				{
					continue;
				}
				const float neighbourLevel = filled[static_cast<std::size_t>(neighbour)];
				const float neighbourElevation = tiles[static_cast<std::size_t>(neighbour)].elevationMeters;
				const bool lowerLevel = neighbourLevel < level - kFlowEpsilonMeters;
				const bool lowerFlat = neighbourLevel <= level + kFlowEpsilonMeters &&
				                       neighbourElevation < tile.elevationMeters - kFlowEpsilonMeters;
				if (!lowerLevel && !lowerFlat)
				{
					continue;
				}
				if (neighbourLevel < bestLevel || (neighbourLevel == bestLevel && neighbourElevation < bestElevation))
				{
					best = neighbour;
					bestLevel = neighbourLevel;
					bestElevation = neighbourElevation;
				}
			}
			downstream[static_cast<std::size_t>(index)] = best;
		}
	}

	// Catchment area, in effective km^2. Each tile contributes its own area
	// weighted by precipitation, so a wet basin feeds a river with a smaller
	// geometric area than a dry one.
	std::vector<float> catchment(count, 0.0f);
	for (int j = -half; j <= half; ++j)
	{
		for (int i = -half; i <= half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			const RegionTile &tile = tiles[static_cast<std::size_t>(index)];
			if (tile.IsOcean())
			{
				continue;
			}
			const float precipitation = std::clamp(tile.precipitationMm / 1000.0f, 0.05f, 4.0f);
			catchment[static_cast<std::size_t>(index)] = kTileAreaKm2 * precipitation;
		}
	}

	std::vector<int> order(count);
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int first, int second) {
		const float firstLevel = filled[static_cast<std::size_t>(first)];
		const float secondLevel = filled[static_cast<std::size_t>(second)];
		if (firstLevel != secondLevel)
		{
			return firstLevel > secondLevel;
		}
		return first < second;
	});

	for (const int index : order)
	{
		const int next = downstream[static_cast<std::size_t>(index)];
		if (next >= 0)
		{
			catchment[static_cast<std::size_t>(next)] += catchment[static_cast<std::size_t>(index)];
		}
	}

	float maximumCatchment = 0.0f;
	for (int j = -half; j <= half; ++j)
	{
		for (int i = -half; i <= half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			if (!tiles[static_cast<std::size_t>(index)].IsOcean())
			{
				maximumCatchment = std::max(maximumCatchment, catchment[static_cast<std::size_t>(index)]);
			}
		}
	}

	for (int j = -half; j <= half; ++j)
	{
		for (int i = -half; i <= half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			RegionTile &tile = tiles[static_cast<std::size_t>(index)];
			if (tile.IsOcean())
			{
				tile.riverDischarge = 0.0f;
				SetFlag(tile.flags, RegionTileFlag::River, false);
				SetFlag(tile.flags, RegionTileFlag::Lake, false);
				continue;
			}

			const float area = catchment[static_cast<std::size_t>(index)];
			tile.riverDischarge = maximumCatchment > 0.0f ? std::clamp(area / maximumCatchment, 0.0f, 1.0f) : 0.0f;
			SetFlag(tile.flags, RegionTileFlag::River, area >= kRiverAreaKm2);

			const float fillDepth = filled[static_cast<std::size_t>(index)] - tile.elevationMeters;
			if (fillDepth > kLakeDepthMeters)
			{
				SetFlag(tile.flags, RegionTileFlag::Lake);
			}
		}
	}
}
} // namespace Infinitesia
