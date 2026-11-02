#pragma once

#include "world/Grid.h"

namespace Infinitesia
{
    enum class BlockType
    {
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
