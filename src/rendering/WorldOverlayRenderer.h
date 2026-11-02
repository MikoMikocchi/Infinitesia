#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"

#include <optional>
#include <vector>

namespace Infinitesia
{
    namespace WorldOverlayRenderer
    {
        void DrawGridLines(
            const Grid& grid,
            GridCell viewCenter,
            int chunkRadius,
            bool showCellGrid,
            int viewLevel);
        void DrawHoveredCell(const Grid& grid, GridCell cell);
        void DrawDebugPath(const Grid& grid, WorldPosition origin, const std::vector<GridCell>& path);
        void DrawObjectHighlights(
            const Grid& grid,
            const World& world,
            std::optional<WorldObjectId> hoveredObject,
            std::optional<WorldObjectId> selectedObject);
    }
}
