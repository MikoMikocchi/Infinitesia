#pragma once

#include <cstdint>

#include "domains/natural/biology/flora/TreeVoxelizer.h"

namespace Infinitesia
{
// The plant of the species that grows from `seed`: its cells relative to the
// base.
TreeVoxels GrowTree(const Species &species, std::uint64_t seed);

// The matter a part of a plant is made of.
MatterId PartMatter(const PlantMatter &matter, TreePart part) noexcept;

// Parts that are wood: they hold the plant together and give logs.
constexpr bool IsWood(TreePart part) noexcept
{
	return part != TreePart::Leaves;
}
} // namespace Infinitesia
