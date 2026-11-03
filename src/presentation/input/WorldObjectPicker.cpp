#include "presentation/input/WorldObjectPicker.h"

#include <limits>

namespace Infinitesia
{
    std::optional<WorldObjectId> WorldObjectPicker::PickObject(
        const Grid& grid,
        const EntityStore& world,
        const Camera3D& camera,
        Vector2 screenPosition,
        int selectedLevel) const
    {
        const Ray ray = GetMouseRay(screenPosition, camera);
        float nearestDistance = std::numeric_limits<float>::max();
        std::optional<WorldObjectId> nearestObject;

        for (const WorldObject& object : world.GetObjects())
        {
            if (object.cell.y != selectedLevel)
            {
                continue;
            }

            const WorldPosition center = object.position;
            const WorldObjectSize size = GetWorldObjectSize(*object.kind, grid.GetCellSize());
            const BoundingBox bounds{
                { center.x - size.width * 0.5f, center.y, center.z - size.width * 0.5f },
                { center.x + size.width * 0.5f, center.y + size.height, center.z + size.width * 0.5f }
            };
            const RayCollision collision = GetRayCollisionBox(ray, bounds);

            if (collision.hit && collision.distance < nearestDistance)
            {
                nearestDistance = collision.distance;
                nearestObject = object.id;
            }
        }

        return nearestObject;
    }
}
