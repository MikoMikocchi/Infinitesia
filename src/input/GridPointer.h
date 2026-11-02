#pragma once

#include "raylib.h"
#include "world/Grid.h"

#include <optional>

namespace Infinitesia
{
    class GridPointer
    {
    public:
        std::optional<GridCell> PickCell(
            const Grid& grid,
            const Camera3D& camera,
            Vector2 screenPosition) const;
    };
}
