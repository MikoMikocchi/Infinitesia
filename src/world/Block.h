#pragma once

#include "world/Grid.h"

namespace Infinitesia
{
    enum class BlockType
    {
        Air,
        Grass,
        Dirt,
        Stone
    };

    struct Block
    {
        GridCell cell;
        BlockType type;
    };
}
