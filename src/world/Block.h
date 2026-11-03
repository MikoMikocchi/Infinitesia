#pragma once

#include "blocks/BlockState.h"
#include "world/Grid.h"

namespace Infinitesia
{
    // One cell of the world: what it is made of and what was made of it. Every rule about the
    // cell (is it empty, can it be walked on, how much light it stops) is the BlockRegistry's
    // answer for `state`.
    struct Block
    {
        GridCell cell;
        BlockState state;
    };
}
