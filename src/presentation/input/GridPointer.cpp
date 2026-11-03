#include "presentation/input/GridPointer.h"

#include <cmath>

namespace Infinitesia
{
    std::optional<GridCell> GridPointer::PickCell(
        const Grid& grid,
        const Camera3D& camera,
        Vector2 screenPosition,
        int selectedLevel) const
    {
        if (selectedLevel < 0 || selectedLevel >= grid.GetHeight())
        {
            return std::nullopt;
        }

        const Ray ray = GetMouseRay(screenPosition, camera);
        if (!std::isfinite(ray.position.y) || !std::isfinite(ray.direction.y) ||
            std::abs(ray.direction.y) <= 1.0e-6f)
        {
            return std::nullopt;
        }

        const float levelHeight =
            static_cast<float>(selectedLevel + 1) * grid.GetCellSize();
        const float distance = (levelHeight - ray.position.y) / ray.direction.y;
        if (!std::isfinite(distance) || distance < 0.0f)
        {
            return std::nullopt;
        }

        const Vector3 intersection{
            ray.position.x + ray.direction.x * distance,
            levelHeight,
            ray.position.z + ray.direction.z * distance
        };
        const std::optional<GridCell> intersectedCell =
            grid.WorldToCell({ intersection.x, intersection.y, intersection.z });
        if (!intersectedCell.has_value())
        {
            return std::nullopt;
        }

        return GridCell{ intersectedCell->x, selectedLevel, intersectedCell->z };
    }
}
