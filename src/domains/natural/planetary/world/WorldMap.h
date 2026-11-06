#pragma once

#include <algorithm>
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
// The world hierarchy of the game (docs/08_WORLD_GRID.md), with one block per
// tile:
//
//   tile (block)          1 m
//   local area block      64 x 64 tiles = 64 m
//   theater tile          16 x 16 local blocks = 1024 x 1024 tiles = 1024 m
//   theater               250 x 250 theater tiles = 256 km
//
// The local hierarchy is deliberately binary: a chunk is 16 blocks, a local
// area block is 4 x 4 chunks, and a theater tile is 64 x 64 chunks. The
// theater is an even-sized square so 250 x 1024 m is exactly 256 km.
inline constexpr int kTilesPerLocalBlock = 64;
inline constexpr int kLocalBlocksPerRegionTile = 16;
inline constexpr int kRegionTilesPerWorldSide = 250;
inline constexpr double kBlockSizeMeters = 1.0;
inline constexpr double kChunkSizeMeters = 16.0 * kBlockSizeMeters;
inline constexpr double kLocalBlockSizeMeters = kTilesPerLocalBlock * kBlockSizeMeters;
inline constexpr double kRegionTileSizeMeters = kLocalBlocksPerRegionTile * kLocalBlockSizeMeters;
inline constexpr double kWorldSizeMeters = kRegionTilesPerWorldSide * kRegionTileSizeMeters;
inline constexpr int kChunksPerLocalBlock = static_cast<int>(kLocalBlockSizeMeters / kChunkSizeMeters);
inline constexpr int kChunksPerRegionTile = kLocalBlocksPerRegionTile * kChunksPerLocalBlock;
inline constexpr int kRegionTilesPerWorldHalfSide = kRegionTilesPerWorldSide / 2;

// Version of the world grid and the terrain derived from it. Raise it whenever
// a change moves theater tiles, region-tile summaries or the local lattice.
inline constexpr std::uint32_t kWorldGridVersion = 2;

static_assert(kRegionTilesPerWorldSide % 2 == 0,
              "The theater uses an even number of tiles so the centre lies between tile -1 and 0.");
static_assert(kTilesPerLocalBlock % 16 == 0, "A local area block must be a whole number of chunks.");

static_assert(LocalGrid::kRegionSizeMeters == kRegionTileSizeMeters,
              "A LocalGrid region must be one theater tile.");

// Address of one theater tile. Signed grid coordinates east and north of the
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

// A river channel inside one theater tile. The endpoints are normalised tile
// coordinates: (0, 0) is the south-west corner, (1, 1) the north-east corner of
// the 1024 m tile. `from` is the upstream end (the border shared with the
// tile that feeds this one, or the centre for a spring), `to` is the
// downstream end. Neighbouring tiles agree on the crossing point because it is
// always the midpoint of the shared border.
struct RiverChannel
{
	float fromX = 0.5f;
	float fromY = 0.5f;
	float toX = 0.5f;
	float toY = 0.5f;
	float widthMeters = 0.0f; // full width across the water surface
	float depthMeters = 0.0f; // incision below the surrounding ground
	// Quadratic Bezier control point in the same 0..1 tile coordinates. A
	// negative pair means a straight segment (synthetic channels and old data).
	float bendX = -1.0f;
	float bendY = -1.0f;

	bool IsValid() const noexcept
	{
		return widthMeters > 0.0f && depthMeters > 0.0f;
	}
	bool HasBend() const noexcept
	{
		return bendX >= 0.0f && bendY >= 0.0f;
	}
};

// What the theater map knows about one theater tile. This is deliberately much
// smaller than PlanetRegion: the theater is 250 x 250 summaries, not a second
// copy of the planet grid.
struct RegionTile
{
	float elevationMeters = 0.0f;
	float temperatureCelsius = 0.0f;
	float precipitationMm = 0.0f;
	float drySeasonMonths = 0.0f;
	float soilFertility = 0.0f;
	float riverDischarge = 0.0f; // normalised 0..1
	// The water surface and its depth at the tile centre. For the sea the level
	// is 0 and the depth is the ocean depth; for a lake it is the spill level
	// from the priority flood and the filled depth; for a river it is the
	// channel's water level and its incision depth. Dry land has the ground as
	// its level and zero depth.
	float waterLevelMeters = 0.0f;
	float waterDepthMeters = 0.0f;
	// Slice of WorldMap::Rivers() holding this tile's channel segments.
	std::uint32_t riverFirst = 0;
	std::uint16_t riverCount = 0;
	// Downstream step to the neighbouring tile: -1, 0 or 1. Zero where nothing
	// flows.
	std::int8_t flowEast = 0;
	std::int8_t flowNorth = 0;
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
	// Whether the tile is covered by standing water or carries a river.
	bool HasWater() const noexcept
	{
		return IsOcean() || IsLake() || (HasRiver() && riverCount > 0);
	}
};

// Where a point lies inside the theater map: the nearest theater tile and the
// normalised coordinates inside it (0 west/south, 1 east/north). `tile` is
// null outside the 250 x 250 window.
struct RegionTileLocation
{
	const RegionTile *tile = nullptr;
	RegionTileId id{};
	double x = 0.0;
	double y = 0.0;

	explicit operator bool() const noexcept
	{
		return tile != nullptr;
	}
};

// A square theater of tiles on the planet, centred on the anchor tile. The
// theater is a local window on the sphere; the planet overview remains a
// separate, coarser grid. The tile range is [-125..124] on both axes.
class WorldMap
{
public:
	static constexpr int kSide = kRegionTilesPerWorldSide;
	static constexpr int kHalfSide = kRegionTilesPerWorldHalfSide;
	static constexpr double kTileSizeMeters = kRegionTileSizeMeters;
	static constexpr double kSizeMeters = kWorldSizeMeters;

	WorldMap() = default;
	WorldMap(LocalGrid grid, std::vector<RegionTile> tiles, std::vector<RiverChannel> rivers = {});

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
	// continuous fields are blended; biome, province, flags and hydrology come
	// from the nearest tile. Empty outside the world window.
	std::optional<RegionTile> SampleAt(CartesianPoint3D direction) const noexcept;

	// The nearest tile and the position inside it. Empty outside the window.
	RegionTileLocation Locate(CartesianPoint3D direction) const noexcept;
	std::span<const RegionTile> Tiles() const noexcept
	{
		return m_tiles;
	}
	std::span<const RiverChannel> Rivers() const noexcept
	{
		return m_rivers;
	}
	std::span<const RiverChannel> RiversOf(const RegionTile &tile) const noexcept
	{
		if (tile.riverCount == 0 || tile.riverFirst >= m_rivers.size())
		{
			return {};
		}
		const std::size_t count = std::min<std::size_t>(tile.riverCount, m_rivers.size() - tile.riverFirst);
		return std::span<const RiverChannel>(m_rivers.data() + tile.riverFirst, count);
	}

private:
	LocalGrid m_grid;
	std::vector<RegionTile> m_tiles;
	std::vector<RiverChannel> m_rivers;
};
} // namespace Infinitesia

template <> struct std::hash<Infinitesia::RegionTileId>
{
	std::size_t operator()(const Infinitesia::RegionTileId &id) const noexcept
	{
		return std::hash<std::uint64_t>{}(id.Pack());
	}
};
