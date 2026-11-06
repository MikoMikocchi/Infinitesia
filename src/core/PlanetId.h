#pragma once

#include <cstdint>
#include <functional>

namespace Infinitesia
{
// A stable address of one planet in a campaign. The home planet is the only one
// generated today; the id is kept so theaters, sites and saves can grow to
// several planets without changing their keys.
using PlanetId = std::uint32_t;
inline constexpr PlanetId kHomePlanet = 1;

// A cell of the planet's latitude/longitude grid. The map itself owns the
// resolution; this type is only an address, not a size.
struct PlanetCellId
{
	int latitude = 0;
	int longitude = 0;

	friend bool operator==(const PlanetCellId &, const PlanetCellId &) = default;

	std::uint64_t Pack() const noexcept
	{
		return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(latitude)) << 32) |
		       static_cast<std::uint64_t>(static_cast<std::uint32_t>(longitude));
	}
};
} // namespace Infinitesia

template <> struct std::hash<Infinitesia::PlanetCellId>
{
	std::size_t operator()(const Infinitesia::PlanetCellId &id) const noexcept
	{
		return std::hash<std::uint64_t>{}(id.Pack());
	}
};
