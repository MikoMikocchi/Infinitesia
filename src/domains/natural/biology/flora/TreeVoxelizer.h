#pragma once

#include "domains/natural/biology/flora/TreeSkeleton.h"

#include <cstdint>
#include <vector>

namespace Infinitesia
{
    struct TreeVoxel
    {
        std::int16_t x = 0;
        std::int16_t y = 0;
        std::int16_t z = 0;
        TreePart part = TreePart::Trunk;
    };

    // The cells of one plant relative to its base, sorted by (y, z, x), one entry per cell.
    struct TreeVoxels
    {
        std::vector<TreeVoxel> cells;
        // Inclusive bounds of the cells; all zero when there are none.
        int minX = 0, maxX = 0;
        int minY = 0, maxY = 0;
        int minZ = 0, maxZ = 0;
    };

    // Turns a skeleton into cells. Wood is rasterised first and wins over foliage where they meet; the
    // ragged edge of every leaf cluster comes from `seed`. Cells above the ground (y >= 0) are all parts but
    // roots; roots take only y < 0.
    TreeVoxels Voxelize(const TreeSkeleton& skeleton, std::uint64_t seed);
}
