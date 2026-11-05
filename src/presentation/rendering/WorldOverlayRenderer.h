#pragma once

#include <optional>
#include <vector>

#include "gameplay/entities/EntityStore.h"
#include "presentation/rendering/ViewMode.h"
#include "raylib.h"
#include "world/Grid.h"

namespace Infinitesia
{
namespace WorldOverlayRenderer
{
void DrawGridLines(const Grid &grid, GridCell viewCenter, int chunkRadius, bool showCellGrid, int viewLevel);
void DrawHoveredCell(const Grid &grid, GridCell cell);
void DrawDebugPath(const Grid &grid, WorldPosition origin, const std::vector<GridCell> &path, int viewLevel,
                   ViewMode mode);
void DrawObjectHighlights(const Grid &grid, const EntityStore &world, std::optional<WorldObjectId> hoveredObject,
                          std::optional<WorldObjectId> selectedObject);
} // namespace WorldOverlayRenderer
} // namespace Infinitesia
