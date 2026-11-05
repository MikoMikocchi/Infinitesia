#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "domains/natural/planetary/PlanetRegion.h"
#include "domains/natural/planetary/region/LocalGrid.h"

namespace Infinitesia
{
// The Dwarf Fortress world hierarchy, with one block per tile
// (docs/08_WORLD_GRID.md):
//
//   tile (block)          1 m
//   local area block      48 x 48 tiles = 48 m
//   region tile           16 x 16 local blocks = 768 x 768 tiles = 768 m
//   world                 257 x 257 region tiles = 197.376 km
//
// The constants are scale-invariant: raising kBlockSizeMeters scales every
// level together. A chunk is 16 blocks, so a local area block is 3 x 3 chunks
// and a region tile is 48 x 48 chunks.
inline constexpr int kTilesPerLocalBlock = 48;
inline constexpr int kLocalBlocksPerRegionTile = 16;
inline constexpr int kRegionTilesPerWorldSide = 257;
inline constexpr double kBlockSizeMeters = 1.0;
inline constexpr double kChunkSizeMeters = 16.0 * kBlockSizeMeters;
inline constexpr double kLocalBlockSizeMeters = kTilesPerLocalBlock * kBlockSizeMeters;
inline constexpr double kRegionTileSizeMeters = kLocalBlocksPerRegionTile * kLocalBlockSizeMeters;
inline constexpr double kWorldSizeMeters = kRegionTilesPerWorldSide * kRegionTileSizeMeters;
inline constexpr int kChunksPerLocalBlock = static_cast<int>(kLocalBlockSizeMeters / kChunkSizeMeters);
inline constexpr int kChunksPerRegionTile = kLocalBlocksPerRegionTile * kChunksPerLocalBlock;
inline constexpr int kRegionTilesPerWorldHalfSide = kRegionTilesPerWorldSide / 2;

static_assert(LocalGrid::kRegionSizeMeters == kRegionTileSizeMeters,
              "A LocalGrid region must be one Dwarf Fortress region tile.");

// Address of one region tile. Signed grid coordinates east and north of the
// anchor tile (0, 0), like RegionId but for the world level.
struct RegionTileId
{
	int i = 0; // east
	int j = 0; // north

	friend bool operator==(const RegionTileId &, const RegionTileId &) = default;

	std::uint64_t Pack() const noexcept
	{
		return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(i)) << 32) |
		       static_cast<std::uint64_t>(static_cast<std::uint32_t>(j));
	}
};

enum class RegionTileFlag : std::uint8_t
{
	None = 0,
	Ocean = 1u << 0,
	Lake = 1u << 1,
	Coast = 1u << 2,
	River = 1u << 3,
};

inline bool HasFlag(std::uint8_t flags, RegionTileFlag flag) noexcept
{
	return (flags & static_cast<std::uint8_t>(flag)) != 0;
}

inline void SetFlag(std::uint8_t &flags, RegionTileFlag flag, bool value = true) noexcept
{
	if (value)
	{
		flags |= static_cast<std::uint8_t>(flag);
	}
	else
	{
		flags &= static_cast<std::uint8_t>(~static_cast<std::uint8_t>(flag));
	}
}

// What the world map knows about one region tile. This is deliberately much
// smaller than PlanetRegion: the world map is 257 x 257 summaries, not a second
// copy of the planet grid.
struct RegionTile
{
	float elevationMeters = 0.0f;
	float temperatureCelsius = 0.0f;
	float precipitationMm = 0.0f;
	float drySeasonMonths = 0.0f;
	float soilFertility = 0.0f;
	float riverDischarge = 0.0f; // normalised 0..1
	Biome biome = Biome::DeepOcean;
	std::uint8_t flags = 0;
	ProvinceId province = kNoProvince;

	bool IsOcean() const noexcept
	{
		return HasFlag(flags, RegionTileFlag::Ocean);
	}
	bool IsLake() const noexcept
	{
		return HasFlag(flags, RegionTileFlag::Lake);
	}
	bool IsCoast() const noexcept
	{
		return HasFlag(flags, RegionTileFlag::Coast);
	}
	bool HasRiver() const noexcept
	{
		return HasFlag(flags, RegionTileFlag::River);
	}
	bool IsLand() const noexcept
	{
		return !IsOcean() && !IsLake();
	}
};

// A square world of region tiles on the planet, centred on the anchor tile.
// The world map is a local window on the sphere; the planet overview remains a
// separate, coarser grid.
class WorldMap
{
public:
	static constexpr int kSide = kRegionTilesPerWorldSide;
	static constexpr int kHalfSide = kRegionTilesPerWorldHalfSide;
	static constexpr double kTileSizeMeters = kRegionTileSizeMeters;
	static constexpr double kSizeMeters = kWorldSizeMeters;

	WorldMap() = default;
	WorldMap(LocalGrid grid, std::vector<RegionTile> tiles);

	const LocalGrid &Grid() const noexcept
	{
		return m_grid;
	}
	int Side() const noexcept
	{
		return kSide;
	}
	std::size_t TileCount() const noexcept
	{
		return m_tiles.size();
	}

	static bool Contains(RegionTileId id) noexcept;
	static std::size_t Index(RegionTileId id) noexcept;

	const RegionTile &At(RegionTileId id) const;
	const RegionTile &At(int i, int j) const
	{
		return At({i, j});
	}

	// nullptr outside the world window.
	const RegionTile *Find(RegionId id) const noexcept;
	const RegionTile *Find(CartesianPoint3D direction) const noexcept;

	// Bilinear (smoothstep) sample of the tile summary at a point. The
	// continuous fields are blended; biome, province and flags come from the
	// nearest tile. Empty outside the world window.
	std::optional<RegionTile> SampleAt(CartesianPoint3D direction) const noexcept;
	std::span<const RegionTile> Tiles() const noexcept
	{
		return m_tiles;
	}

private:
	LocalGrid m_grid;
	std::vector<RegionTile> m_tiles;
};
} // namespace Infinitesia

template <> struct std::hash<Infinitesia::RegionTileId>
{
	std::size_t operator()(const Infinitesia::RegionTileId &id) const noexcept
	{
		return std::hash<std::uint64_t>{}(id.Pack());
	}
};
