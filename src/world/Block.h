#pragma once

#include "geology/catalog/MaterialId.h"
#include "world/Grid.h"

namespace Infinitesia
{
    enum class BlockType
    {
        Air,
        Grass,
        Dirt,
        Stone,
        Sand,
        Water
    };

    struct Block
    {
        GridCell cell;
        BlockType type;
        // What the block is made of when the terrain knows it (the rock of a stone block);
        // kUnspecifiedMaterial leaves it to the block type.
        MaterialId material = kUnspecifiedMaterial;
    };
}
