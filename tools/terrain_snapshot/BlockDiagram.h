#pragma once

#include "terrain_snapshot/Options.h"
#include "terrain_snapshot/SnapshotWorld.h"

namespace SnapshotTool
{
    // The isometric block diagram: the ground from above together with the cut faces of the block.
    int RenderBlockDiagram(const Options& options, const Window& window, const SnapshotWorld& world);
}
