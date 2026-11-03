#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/VoxelWorld.h"
#include "presentation/rendering/ViewMode.h"

#include <optional>

namespace Infinitesia
{
    class GridPointer
    {
    public:
        // The first solid block under the cursor that the view shows: with the cut view,
        // nothing above the selected level can be hit.
        std::optional<GridCell> PickCell(
            const Grid& grid,
            const VoxelWorld& terrain,
            const Camera3D& camera,
            Vector2 screenPosition,
            int selectedLevel,
            ViewMode mode) const;
    };
}
