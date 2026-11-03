#pragma once

#include "gameplay/items/Inventory.h"
#include "gameplay/items/Item.h"
#include "world/Grid.h"

#include <cstdint>
#include <optional>

namespace Infinitesia
{
    enum class WorldObjectType
    {
        Worker,
        Tree,
        Item
    };

    struct WorldObjectId
    {
        std::uint64_t value;

        bool operator==(const WorldObjectId&) const = default;
    };

    struct WorldObjectPlacement
    {
        WorldObjectType type;
        GridCell cell;
        std::optional<Item> item = std::nullopt;
    };

    struct WorldObject
    {
        WorldObjectId id;
        WorldObjectType type;
        GridCell cell;
        WorldPosition position;
        std::optional<Item> item;
        Inventory inventory;
    };

    struct WorldObjectSize
    {
        float width;
        float height;
    };

    WorldObjectSize GetWorldObjectSize(WorldObjectType type, float cellSize);
}
