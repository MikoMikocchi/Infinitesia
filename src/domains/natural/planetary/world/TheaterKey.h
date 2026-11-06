#pragma once

#include <cstddef>
#include <cstdint>

#include "core/PlanetId.h"

namespace Infinitesia
{
// Address of one theater: a planet cell whose centre is the anchor of the
// 250 x 250 theater window. The planet is part of the key so theaters of
// different planets can never alias even when their cell indices coincide.
struct TheaterKey
{
	PlanetId planet = kHomePlanet;
	PlanetCellId cell{};

	friend bool operator==(const TheaterKey &, const TheaterKey &) = default;

	std::uint64_t Pack() const noexcept
	{
		return cell.Pack() ^ (static_cast<std::uint64_t>(planet) << 32);
	}
};
} // namespace Infinitesia

template <> struct std::hash<Infinitesia::TheaterKey>
{
	std::size_t operator()(const Infinitesia::TheaterKey &key) const noexcept
	{
		// Mix the planet into the cell hash; both halves of the key must take
		// part, otherwise two planets would evict each other's theaters.
		const std::size_t cell = std::hash<Infinitesia::PlanetCellId>{}(key.cell);
		const std::size_t planet = std::hash<Infinitesia::PlanetId>{}(key.planet);
		return cell ^ (planet + 0x9E3779B97F4A7C15ull + (cell << 6) + (cell >> 2));
	}
};
