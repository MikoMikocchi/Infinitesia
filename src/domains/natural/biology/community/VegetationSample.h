#pragma once

#include <array>
#include <cstdint>

namespace Infinitesia
{
inline constexpr int kVegetationSpeciesPerCell = 4;
inline constexpr std::uint16_t kNoSpecies = 0xFFFF;

// What the stage `vegetation` writes for one cell of the planet
// (docs/07_BIOLOGY.md, section 5.2): the share of the ground that a forest
// shades and up to four kinds of trees that make it up, as indexes into the
// SpeciesCatalog the planet was built with, with weights that add up to 1.
struct VegetationCell
{
	float canopyCover = 0.0f; // 0..1
	std::array<std::uint16_t, kVegetationSpeciesPerCell> species{kNoSpecies, kNoSpecies, kNoSpecies, kNoSpecies};
	std::array<float, kVegetationSpeciesPerCell> weight{};
};

// The vegetation at one point of the planet: the cells around it blended, so
// forests change gradually and not along the borders of the 55 km cells.
struct VegetationSample
{
	struct Entry
	{
		std::uint16_t species = kNoSpecies;
		double weight = 0.0;
	};

	double canopyCover = 0.0;
	// Weights add up to 1 when there are any entries.
	std::array<Entry, 4 * kVegetationSpeciesPerCell> entries{};
	int count = 0;
};
} // namespace Infinitesia
