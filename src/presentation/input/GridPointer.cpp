#include "presentation/input/GridPointer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Infinitesia
{
namespace
{
constexpr float kInfinity = std::numeric_limits<float>::infinity();

// The step in t to cross one cell along an axis, and the t of the first cell
// border.
void InitAxis(float origin, float direction, int cell, float &nextT, float &stepT, int &step)
{
	if (std::abs(direction) < 1.0e-9f)
	{
		nextT = kInfinity;
		stepT = kInfinity;
		step = 0;
		return;
	}
	step = direction > 0.0f ? 1 : -1;
	stepT = 1.0f / std::abs(direction);
	const float border = static_cast<float>(direction > 0.0f ? cell + 1 : cell);
	nextT = (border - origin) / direction;
}
} // namespace

std::optional<GridCell> GridPointer::PickCell(const Grid &grid, const VoxelWorld &terrain, const Camera3D &camera,
                                              Vector2 screenPosition, int selectedLevel, ViewMode mode) const
{
	if (selectedLevel < 0 || selectedLevel >= grid.GetHeight())
	{
		return std::nullopt;
	}

	const Ray ray = GetMouseRay(screenPosition, camera);
	if (!std::isfinite(ray.position.x) || !std::isfinite(ray.position.y) || !std::isfinite(ray.position.z) ||
	    !std::isfinite(ray.direction.x) || !std::isfinite(ray.direction.y) || !std::isfinite(ray.direction.z))
	{
		return std::nullopt;
	}

	// The ray in cell units, with the grid's corner at the origin; the part of
	// the grid the view shows is the box [0, size] on each axis.
	const float cellSize = grid.GetCellSize();
	const float origin[3]{(ray.position.x + grid.GetWorldWidth() * 0.5f) / cellSize, ray.position.y / cellSize,
	                      (ray.position.z + grid.GetWorldDepth() * 0.5f) / cellSize};
	const float direction[3]{ray.direction.x, ray.direction.y, ray.direction.z};
	const int shownHeight = mode == ViewMode::CutAtLevel ? selectedLevel + 1 : grid.GetHeight();
	const int size[3]{grid.GetWidth(), shownHeight, grid.GetDepth()};

	float enter = 0.0f;
	float leave = kInfinity;
	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(direction[axis]) < 1.0e-9f)
		{
			if (origin[axis] < 0.0f || origin[axis] > static_cast<float>(size[axis]))
			{
				return std::nullopt;
			}
			continue;
		}
		float near = (0.0f - origin[axis]) / direction[axis];
		float far = (static_cast<float>(size[axis]) - origin[axis]) / direction[axis];
		if (near > far)
		{
			std::swap(near, far);
		}
		enter = std::max(enter, near);
		leave = std::min(leave, far);
	}
	if (enter > leave)
	{
		return std::nullopt;
	}

	// Walk the cells along the ray from where it enters the shown box.
	const float entryT = enter + 1.0e-4f;
	int cell[3];
	float nextT[3];
	float stepT[3];
	int step[3];
	for (int axis = 0; axis < 3; ++axis)
	{
		const float entry = origin[axis] + direction[axis] * entryT;
		cell[axis] = std::clamp(static_cast<int>(std::floor(entry)), 0, size[axis] - 1);
		InitAxis(entry, direction[axis], cell[axis], nextT[axis], stepT[axis], step[axis]);
	}

	const int maxSteps = size[0] + size[1] + size[2] + 3;
	for (int walked = 0; walked < maxSteps; ++walked)
	{
		const GridCell candidate{cell[0], cell[1], cell[2]};
		const Block *block = terrain.GetBlock(candidate);
		if (block != nullptr && terrain.Registry().Occupied(block->state))
		{
			return candidate;
		}

		int axis = 0;
		if (nextT[1] < nextT[axis])
		{
			axis = 1;
		}
		if (nextT[2] < nextT[axis])
		{
			axis = 2;
		}
		if (step[axis] == 0)
		{
			break;
		}
		cell[axis] += step[axis];
		nextT[axis] += stepT[axis];
		if (cell[axis] < 0 || cell[axis] >= size[axis])
		{
			break;
		}
	}
	return std::nullopt;
}
} // namespace Infinitesia
