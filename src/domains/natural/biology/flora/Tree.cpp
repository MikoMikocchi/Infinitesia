#include "domains/natural/biology/flora/Tree.h"

namespace Infinitesia
{
    TreeVoxels GrowTree(const Species& species, std::uint64_t seed)
    {
        return Voxelize(GrowSkeleton(species, seed), seed);
    }

    MaterialId PartMaterial(const PlantMaterials& materials, TreePart part) noexcept
    {
        switch (part)
        {
        case TreePart::Trunk:  return materials.trunk;
        case TreePart::Branch: return materials.branch;
        case TreePart::Twig:   return materials.twig;
        case TreePart::Leaves: return materials.leaves;
        case TreePart::Root:   return materials.root;
        }
        return kUnspecifiedMaterial;
    }
}
