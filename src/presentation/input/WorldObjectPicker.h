#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "gameplay/entities/EntityStore.h"

#include <optional>

namespace Infinitesia
{
    class WorldObjectPicker
    {
    public:
        std::optional<WorldObjectId> PickObject(
            const Grid& grid,
            const EntityStore& world,
            const Camera3D& camera,
            Vector2 screenPosition,
            int selectedLevel) const;
    };
}
