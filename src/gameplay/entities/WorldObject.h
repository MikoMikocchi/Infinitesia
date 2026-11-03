#pragma once

#include "gameplay/content/ContentTypes.h"
#include "gameplay/items/Inventory.h"
#include "gameplay/items/Item.h"
#include "world/Grid.h"

#include <cstdint>
#include <optional>

namespace Infinitesia
{
    struct WorldObjectId
    {
        std::uint64_t value;

        bool operator==(const WorldObjectId&) const = default;
    };

    struct WorldObjectPlacement
    {
        const ObjectKindDef* kind;
        GridCell cell;
        std::optional<Item> item = std::nullopt;
    };

    // An object of the world. Its kind (from the GameContent, which must outlive it) says
    // what it is and what can be done with it.
    struct WorldObject
    {
        WorldObjectId id;
        const ObjectKindDef* kind;
        GridCell cell;
        WorldPosition position;
        std::optional<Item> item;
        Inventory inventory;

        bool Is(ObjectAbility ability) const { return kind->Has(ability); }
    };

    struct WorldObjectSize
    {
        float width;
        float height;
    };

    WorldObjectSize GetWorldObjectSize(const ObjectKindDef& kind, float cellSize);
}
