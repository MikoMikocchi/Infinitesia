#pragma once

#include "terrain_snapshot/Options.h"
#include "terrain_snapshot/SnapshotWorld.h"

namespace SnapshotTool
{
// A geological map from above: the rock under the topsoil of every column,
// north up.
int RenderGeologicalMap(const Options &options, const Window &window, const SnapshotWorld &world);
} // namespace SnapshotTool
