#pragma once

#include <optional>

#include "gameplay/entities/EntityStore.h"
#include "presentation/rendering/ViewMode.h"
#include "raylib.h"
#include "world/Grid.h"

namespace Infinitesia {
class WorldObjectPicker {
 public:
  std::optional<WorldObjectId> PickObject(
      const Grid& grid, const EntityStore& world, const Camera3D& camera,
      Vector2 screenPosition, int selectedLevel, ViewMode mode) const;
};
}  // namespace Infinitesia
