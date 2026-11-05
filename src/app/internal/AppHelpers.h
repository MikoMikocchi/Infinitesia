#pragma once

#include "presentation/rendering/WorldRenderer.h"
#include "world/Grid.h"

namespace Infinitesia
{
namespace AppInternal
{
void UpdateLevelSelection(WorldRenderer &worldRenderer, Grid &grid, int &selectedLevel);
}
} // namespace Infinitesia
