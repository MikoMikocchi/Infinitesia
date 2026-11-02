#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "world/World.h"

#include <optional>

namespace Infinitesia
{
    class WorldObjectPicker
    {
    public:
        std::optional<WorldObjectId> PickObject(
            const Grid& grid,
            const World& world,
            const Camera3D& camera,
            Vector2 screenPosition) const;
    };
}
