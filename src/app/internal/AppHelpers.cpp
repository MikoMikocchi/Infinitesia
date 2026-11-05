#include "app/internal/AppHelpers.h"

#include <algorithm>

#include "raylib.h"

namespace Infinitesia
{
namespace AppInternal
{
void UpdateLevelSelection(WorldRenderer &worldRenderer, Grid &grid, int &selectedLevel)
{
	int nextLevel = selectedLevel;
	if (IsKeyPressed(KEY_PAGE_DOWN))
	{
		nextLevel = std::max(0, selectedLevel - 1);
	}
	else if (IsKeyPressed(KEY_PAGE_UP))
	{
		nextLevel = std::min(grid.GetHeight() - 1, selectedLevel + 1);
	}
	if (nextLevel != selectedLevel)
	{
		Camera3D &camera = worldRenderer.GetCamera();
		const float levelHeight = static_cast<float>(nextLevel + 1) * grid.GetCellSize();
		const float verticalOffset = levelHeight - camera.target.y;
		camera.target.y = levelHeight;
		camera.position.y += verticalOffset;
		selectedLevel = nextLevel;
	}
}
} // namespace AppInternal
} // namespace Infinitesia
