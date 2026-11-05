#include "domains/natural/planetary/world/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
WorldMap::WorldMap(LocalGrid grid, std::vector<RegionTile> tiles) : m_grid(std::move(grid)), m_tiles(std::move(tiles))
{
	if (m_tiles.size() != static_cast<std::size_t>(kSide) * static_cast<std::size_t>(kSide))
	{
		throw std::invalid_argument("A world map must hold exactly 257 x 257 region tiles.");
	}
}

bool WorldMap::Contains(RegionTileId id) noexcept
{
	return id.i >= -kHalfSide && id.i <= kHalfSide && id.j >= -kHalfSide && id.j <= kHalfSide;
}

std::size_t WorldMap::Index(RegionTileId id) noexcept
{
	const int x = id.i + kHalfSide;
	const int y = id.j + kHalfSide;
	return static_cast<std::size_t>(y) * static_cast<std::size_t>(kSide) + static_cast<std::size_t>(x);
}

const RegionTile &WorldMap::At(RegionTileId id) const
{
	if (!Contains(id) || m_tiles.empty())
	{
		throw std::out_of_range("Region tile is outside the world map.");
	}
	return m_tiles[Index(id)];
}

const RegionTile *WorldMap::Find(RegionId id) const noexcept
{
	const RegionTileId tile{id.i, id.j};
	return Contains(tile) && !m_tiles.empty() ? &m_tiles[Index(tile)] : nullptr;
}

const RegionTile *WorldMap::Find(CartesianPoint3D direction) const noexcept
{
	return Find(m_grid.RegionAt(direction));
}

std::optional<RegionTile> WorldMap::SampleAt(CartesianPoint3D direction) const noexcept
{
	if (m_tiles.empty())
	{
		return std::nullopt;
	}

	double eastMeters = 0.0;
	double northMeters = 0.0;
	m_grid.Project(direction, eastMeters, northMeters);
	const double tileX = eastMeters / kRegionTileSizeMeters;
	const double tileY = northMeters / kRegionTileSizeMeters;
	// Match Locate: a point may lie in the outer half of the edge tiles.
	constexpr double kEdge = static_cast<double>(kHalfSide) + 0.5;
	if (tileX < -kEdge || tileX > kEdge || tileY < -kEdge || tileY > kEdge)
	{
		return std::nullopt;
	}

	const int x0 = static_cast<int>(std::floor(tileX));
	const int y0 = static_cast<int>(std::floor(tileY));
	const int x1 = std::clamp(x0 + 1, -kHalfSide, kHalfSide);
	const int y1 = std::clamp(y0 + 1, -kHalfSide, kHalfSide);
	const int cx0 = std::clamp(x0, -kHalfSide, kHalfSide);
	const int cy0 = std::clamp(y0, -kHalfSide, kHalfSide);

	const float tx = static_cast<float>(tileX - x0);
	const float ty = static_cast<float>(tileY - y0);
	// Smoothstep interpolation keeps the derivative zero at tile borders, so the
	// hill-shaded map has no crease where two world tiles meet.
	const float sx = tx * tx * (3.0f - 2.0f * tx);
	const float sy = ty * ty * (3.0f - 2.0f * ty);

	const RegionTile &a = m_tiles[Index({cx0, cy0})];
	const RegionTile &b = m_tiles[Index({x1, cy0})];
	const RegionTile &c = m_tiles[Index({cx0, y1})];
	const RegionTile &d = m_tiles[Index({x1, y1})];
	const auto blend = [&](float av, float bv, float cv, float dv) {
		const float bottom = av + (bv - av) * sx;
		const float top = cv + (dv - cv) * sx;
		return bottom + (top - bottom) * sy;
	};

	RegionTile result;
	result.elevationMeters = blend(a.elevationMeters, b.elevationMeters, c.elevationMeters, d.elevationMeters);
	result.temperatureCelsius =
	    blend(a.temperatureCelsius, b.temperatureCelsius, c.temperatureCelsius, d.temperatureCelsius);
	result.precipitationMm = blend(a.precipitationMm, b.precipitationMm, c.precipitationMm, d.precipitationMm);
	result.drySeasonMonths = blend(a.drySeasonMonths, b.drySeasonMonths, c.drySeasonMonths, d.drySeasonMonths);
	result.soilFertility = blend(a.soilFertility, b.soilFertility, c.soilFertility, d.soilFertility);
	result.riverDischarge = blend(a.riverDischarge, b.riverDischarge, c.riverDischarge, d.riverDischarge);

	const int nearestX = std::clamp(static_cast<int>(std::lround(tileX)), -kHalfSide, kHalfSide);
	const int nearestY = std::clamp(static_cast<int>(std::lround(tileY)), -kHalfSide, kHalfSide);
	const RegionTile &nearest = m_tiles[Index({nearestX, nearestY})];
	result.biome = nearest.biome;
	result.province = nearest.province;
	result.flags = nearest.flags;
	// Water bodies are not smooth fields: a lake keeps one level to its shore
	// and a river channel belongs to the tile it runs through.
	result.waterLevelMeters = nearest.waterLevelMeters;
	result.waterDepthMeters = nearest.waterDepthMeters;
	result.river = nearest.river;
	result.flowEast = nearest.flowEast;
	result.flowNorth = nearest.flowNorth;
	return result;
}

RegionTileLocation WorldMap::Locate(CartesianPoint3D direction) const noexcept
{
	if (m_tiles.empty())
	{
		return {};
	}

	double eastMeters = 0.0;
	double northMeters = 0.0;
	m_grid.Project(direction, eastMeters, northMeters);
	const double tileX = eastMeters / kRegionTileSizeMeters;
	const double tileY = northMeters / kRegionTileSizeMeters;
	// Tiles are centred on integer coordinates and span half a tile to each
	// side, so the outer edge of the window is at +/- (kHalfSide + 0.5).
	constexpr double kEdge = static_cast<double>(kHalfSide) + 0.5;
	if (tileX < -kEdge || tileX > kEdge || tileY < -kEdge || tileY > kEdge)
	{
		return {};
	}

	const RegionTileId id{static_cast<int>(std::floor(tileX + 0.5)), static_cast<int>(std::floor(tileY + 0.5))};
	if (!Contains(id))
	{
		return {};
	}

	RegionTileLocation location;
	location.id = id;
	location.tile = &m_tiles[Index(id)];
	location.x = std::clamp(tileX - (static_cast<double>(id.i) - 0.5), 0.0, 1.0);
	location.y = std::clamp(tileY - (static_cast<double>(id.j) - 0.5), 0.0, 1.0);
	return location;
}
} // namespace Infinitesia
