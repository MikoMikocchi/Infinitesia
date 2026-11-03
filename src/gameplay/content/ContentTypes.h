#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Infinitesia
{
    // How the presentation draws an item; the data picks one of the looks the game has.
    enum class ItemVisual : std::uint8_t { Generic, Logs };

    struct ItemDef
    {
        std::string key;       // "item.logs"
        std::string name;
        int maxStack = 64;
        ItemVisual visual = ItemVisual::Generic;
    };

    // How the presentation draws an object of this kind.
    enum class ObjectVisual : std::uint8_t { Worker, Tree, Item };

    // What the rules let an object of a kind do or be done to; the work system and the
    // commands ask for abilities, never for a particular kind.
    enum class ObjectAbility : std::uint8_t
    {
        Worker = 1,     // takes orders, carries an inventory
        Choppable = 2,  // can be chopped down (see ObjectKindDef::chopped)
        Pickable = 4    // a worker can pick it up
    };

    struct ObjectKindDef;

    // What a chopped object turns into.
    struct ChoppedResult
    {
        const ObjectKindDef* becomes = nullptr;
        const ItemDef* item = nullptr;
    };

    struct ObjectKindDef
    {
        std::string key;       // "object.tree"
        std::string name;
        float widthInCells = 1.0f;
        float heightInCells = 1.0f;
        ObjectVisual visual = ObjectVisual::Item;
        std::uint8_t abilities = 0;
        bool carriesItem = false; // such an object holds an item (a pile on the ground)
        int shadowOpacity = 110;  // 0..255
        std::optional<ChoppedResult> chopped;

        bool Has(ObjectAbility ability) const
        {
            return (abilities & static_cast<std::uint8_t>(ability)) != 0;
        }
    };
}
