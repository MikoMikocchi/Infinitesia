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
// Water sits this far below the surrounding ground so the banks stay visible.
constexpr float kRiverFreeboardMeters = 0.25f;

// A river occupies a channel in its theater tile. Width follows the square root
// of the catchment (a river twice as wide drains four times the area). The
// result approaches a DF-like major-river width of 40 m instead of clamping
// every large river to the same value, so tributaries and the main stem keep
// their differences. The half-value is at raw = 40 (a catchment of about
// 256 km2); the 40 m asymptote is only reached by the largest systems.
constexpr float kMaximumRiverWidthMeters = 40.0f;

float RiverWidthMeters(float catchmentKm2) noexcept
{
	const float raw = 2.5f * std::sqrt(std::max(catchmentKm2, 0.0f));
	const float saturated = raw * kMaximumRiverWidthMeters / (raw + kMaximumRiverWidthMeters);
	return std::clamp(saturated, 3.0f, kMaximumRiverWidthMeters);
}

float RiverDepthMeters(float widthMeters) noexcept
{
	return std::clamp(0.4f + 0.05f * widthMeters, 1.25f, 5.0f);
}

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

struct TilePoint
{
	float x = 0.5f;
	float y = 0.5f;
};

// Point on the border of the `to` tile that the flow from `from` crosses.
// Orthogonal crossings are border midpoints, diagonal crossings are corners, so
// the two tiles sharing the border agree on the point exactly.
TilePoint BorderPoint(int fromI, int fromJ, int toI, int toJ) noexcept
{
	const int dx = fromI - toI;
	const int dy = fromJ - toJ;
	return {dx < 0 ? 0.0f : (dx > 0 ? 1.0f : 0.5f), dy < 0 ? 0.0f : (dy > 0 ? 1.0f : 0.5f)};
}

void AddChannel(std::vector<RegionTile> &tiles, std::vector<RiverChannel> &rivers, int index, RiverChannel channel)
{
	const float dx = channel.toX - channel.fromX;
	const float dy = channel.toY - channel.fromY;
	if (dx * dx + dy * dy < 1.0e-6f)
	{
		return;
	}
	RegionTile &tile = tiles[static_cast<std::size_t>(index)];
	if (tile.riverCount == 0)
	{
		tile.riverFirst = static_cast<std::uint32_t>(rivers.size());
	}
	rivers.push_back(channel);
	++tile.riverCount;
}

float MeanderHash(int i, int j, int segment) noexcept
{
	std::uint32_t hash = static_cast<std::uint32_t>(i) * 0x9E3779B1u;
	hash ^= static_cast<std::uint32_t>(j) * 0x85EBCA77u;
	hash ^= static_cast<std::uint32_t>(segment) * 0xC2B2AE3Du;
	hash ^= hash >> 16;
	hash *= 0x7FEB352Du;
	hash ^= hash >> 15;
	return static_cast<float>(hash & 0xFFFFu) / 65535.0f;
}

// A quadratic Bezier control point. The meander amplitude is a fraction of the
// segment length, so short tributary stubs stay tame while the main stem can
// curve across the tile. The control point is clamped inside the tile.
TilePoint MeanderControl(TilePoint from, TilePoint to, int i, int j, int segment) noexcept
{
	const double dx = static_cast<double>(to.x) - from.x;
	const double dy = static_cast<double>(to.y) - from.y;
	const double length = std::hypot(dx, dy);
	if (length < 1.0e-4)
	{
		return {0.5f, 0.5f};
	}

	const double nx = -dy / length;
	const double ny = dx / length;
	const double amplitude = std::min(length * 0.18, 0.12);
	const double phase = 2.0 * static_cast<double>(MeanderHash(i, j, segment)) - 1.0;
	TilePoint control{static_cast<float>((from.x + to.x) * 0.5 + nx * amplitude * phase),
	                  static_cast<float>((from.y + to.y) * 0.5 + ny * amplitude * phase)};
	control.x = std::clamp(control.x, 0.02f, 0.98f);
	control.y = std::clamp(control.y, 0.02f, 0.98f);
	return control;
}

RiverChannel ChannelBetween(TilePoint from, TilePoint to, float catchmentKm2, TilePoint control) noexcept
{
	RiverChannel channel;
	channel.fromX = from.x;
	channel.fromY = from.y;
	channel.toX = to.x;
	channel.toY = to.y;
	channel.widthMeters = RiverWidthMeters(catchmentKm2);
	channel.depthMeters = RiverDepthMeters(channel.widthMeters);
	channel.bendX = control.x;
	channel.bendY = control.y;
	return channel;
}
} // namespace

std::vector<RiverChannel> ComputeWorldHydrology(std::vector<RegionTile> &tiles)
{
	std::vector<RiverChannel> rivers;
	constexpr int side = kRegionTilesPerWorldSide;
	constexpr int half = kRegionTilesPerWorldHalfSide;
	const std::size_t count = static_cast<std::size_t>(side) * static_cast<std::size_t>(side);
	if (tiles.size() != count)
	{
		return rivers;
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

	for (int j = -half; j < half; ++j)
	{
		for (int i = -half; i < half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			const RegionTile &tile = tiles[static_cast<std::size_t>(index)];
			const bool edge = i == -half || i == half - 1 || j == -half || j == half - 1;
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
	for (int j = -half; j < half; ++j)
	{
		for (int i = -half; i < half; ++i)
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
	for (int j = -half; j < half; ++j)
	{
		for (int i = -half; i < half; ++i)
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
	for (int j = -half; j < half; ++j)
	{
		for (int i = -half; i < half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			if (!tiles[static_cast<std::size_t>(index)].IsOcean())
			{
				maximumCatchment = std::max(maximumCatchment, catchment[static_cast<std::size_t>(index)]);
			}
		}
	}

	for (int j = -half; j < half; ++j)
	{
		for (int i = -half; i < half; ++i)
		{
			const int index = static_cast<int>(WorldMap::Index({i, j}));
			RegionTile &tile = tiles[static_cast<std::size_t>(index)];
			tile.riverFirst = 0;
			tile.riverCount = 0;

			if (tile.IsOcean())
			{
				tile.riverDischarge = 0.0f;
				SetFlag(tile.flags, RegionTileFlag::River, false);
				SetFlag(tile.flags, RegionTileFlag::Lake, false);
				tile.waterLevelMeters = 0.0f;
				tile.waterDepthMeters = std::max(0.0f, -tile.elevationMeters);
				tile.flowEast = 0;
				tile.flowNorth = 0;
				continue;
			}

			const float area = catchment[static_cast<std::size_t>(index)];
			tile.riverDischarge = maximumCatchment > 0.0f ? std::clamp(area / maximumCatchment, 0.0f, 1.0f) : 0.0f;
			SetFlag(tile.flags, RegionTileFlag::River, area >= kRiverAreaKm2);

			const float fillDepth = filled[static_cast<std::size_t>(index)] - tile.elevationMeters;
			SetFlag(tile.flags, RegionTileFlag::Lake, fillDepth > kLakeDepthMeters);

			// Flow direction is the step to the downstream tile. It is kept even
			// on a lake surface, where the water itself is flat.
			const int next = downstream[static_cast<std::size_t>(index)];
			tile.flowEast = 0;
			tile.flowNorth = 0;
			int stepI = 0;
			int stepJ = 0;
			if (next >= 0)
			{
				const int nextI = next % side - half;
				const int nextJ = next / side - half;
				stepI = nextI - i;
				stepJ = nextJ - j;
				tile.flowEast = static_cast<std::int8_t>(stepI > 0 ? 1 : (stepI < 0 ? -1 : 0));
				tile.flowNorth = static_cast<std::int8_t>(stepJ > 0 ? 1 : (stepJ < 0 ? -1 : 0));
			}

			if (tile.IsLake())
			{
				// The priority flood found the spill level: the local terrain fills
				// up to it, leaving islands and shores where the ground is higher.
				tile.waterLevelMeters = filled[static_cast<std::size_t>(index)];
				tile.waterDepthMeters = std::max(0.0f, fillDepth);
				continue;
			}

			if (!tile.HasRiver() || next < 0)
			{
				tile.waterLevelMeters = tile.elevationMeters;
				tile.waterDepthMeters = 0.0f;
				continue;
			}

			const TilePoint outlet = BorderPoint(next % side - half, next / side - half, i, j);

			// Every upstream river tile enters through the shared border. The
			// largest channel still defines the visible main stem, but all
			// tributaries now get their own segment to the junction.
			struct Inlet
			{
				TilePoint point;
				float areaKm2 = 0.0f;
			};
			Inlet inlets[8];
			int inletCount = 0;
			for (int direction = 0; direction < 8; ++direction)
			{
				const int neighbour = NeighbourIndex(i, j, deltaI[direction], deltaJ[direction]);
				if (neighbour < 0 || downstream[static_cast<std::size_t>(neighbour)] != index)
				{
					continue;
				}
				const float neighbourArea = catchment[static_cast<std::size_t>(neighbour)];
				if (neighbourArea < kRiverAreaKm2)
				{
					continue;
				}
				const int neighbourI = neighbour % side - half;
				const int neighbourJ = neighbour / side - half;
				inlets[inletCount++] = {BorderPoint(neighbourI, neighbourJ, i, j), neighbourArea};
			}

			const float width = RiverWidthMeters(area);
			const float depth = RiverDepthMeters(width);
			int segment = 0;
			if (inletCount == 0)
			{
				// A spring at the tile centre.
				const TilePoint source{0.5f, 0.5f};
				AddChannel(tiles, rivers, index,
				           ChannelBetween(source, outlet, area, MeanderControl(source, outlet, i, j, segment)));
			}
			else if (inletCount == 1)
			{
				AddChannel(tiles, rivers, index,
				           ChannelBetween(inlets[0].point, outlet, area,
				                          MeanderControl(inlets[0].point, outlet, i, j, segment)));
			}
			else
			{
				// A junction in the middle of the tile: each tributary keeps its
				// own width up to the junction, and the combined channel leaves
				// the tile at the catchment width.
				const TilePoint junction{0.5f, 0.5f};
				for (int inlet = 0; inlet < inletCount; ++inlet)
				{
					AddChannel(tiles, rivers, index,
					           ChannelBetween(inlets[inlet].point, junction, inlets[inlet].areaKm2,
					                          MeanderControl(inlets[inlet].point, junction, i, j, segment++)));
				}
				AddChannel(tiles, rivers, index,
				           ChannelBetween(junction, outlet, area, MeanderControl(junction, outlet, i, j, segment)));
			}

			tile.waterLevelMeters = tile.elevationMeters - kRiverFreeboardMeters;
			tile.waterDepthMeters = depth;
		}
	}

	return rivers;
}
} // namespace Infinitesia
