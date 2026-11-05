#include "domains/natural/biology/flora/Tree.h"

namespace Infinitesia
{
TreeVoxels GrowTree(const Species &species, std::uint64_t seed)
{
	return Voxelize(GrowSkeleton(species, seed), seed);
}

MatterId PartMatter(const PlantMatter &matter, TreePart part) noexcept
{
	switch (part)
	{
	case TreePart::Trunk:
		return matter.trunk;
	case TreePart::Branch:
		return matter.branch;
	case TreePart::Twig:
		return matter.twig;
	case TreePart::Leaves:
		return matter.leaves;
	case TreePart::Root:
		return matter.root;
	}
	return kUnspecifiedMatter;
}
} // namespace Infinitesia
