#include "input/GridPointer.h"

#include <cmath>

namespace Infinitesia
{
    std::optional<GridCell> GridPointer::PickCell(
        const Grid& grid,
        const Camera3D& camera,
        Vector2 screenPosition) const
    {
        const Ray ray = GetMouseRay(screenPosition, camera);
        if (!std::isfinite(ray.position.y) || !std::isfinite(ray.direction.y) ||
            std::abs(ray.direction.y) <= 1.0e-6f)
        {
            return std::nullopt;
        }

        const float surfaceHeight = grid.GetWorldHeight();
        const float distance = (surfaceHeight - ray.position.y) / ray.direction.y;
        if (!std::isfinite(distance) || distance < 0.0f)
        {
            return std::nullopt;
        }

        const Vector3 intersection{
            ray.position.x + ray.direction.x * distance,
            surfaceHeight,
            ray.position.z + ray.direction.z * distance
        };
        return grid.WorldToCell({ intersection.x, intersection.y, intersection.z });
    }
}
