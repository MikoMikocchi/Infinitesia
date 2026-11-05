#include "domains/natural/planetary/world/WorldMap.h"

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
} // namespace Infinitesia
