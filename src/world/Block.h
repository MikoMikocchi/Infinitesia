#pragma once

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
    };
}
