#pragma once

#include "world/Item.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace Infinitesia
{
    class Inventory
    {
    public:
        static constexpr std::size_t kInitialSlotCount = 4;
        static constexpr int kMaxStackSize = 64;

        const std::vector<std::optional<Item>>& GetSlots() const;
        void AddSlots(std::size_t count);
        bool CanStore(const Item& item) const;
        bool TryAdd(const Item& item);

    private:
        std::vector<std::optional<Item>> m_slots =
            std::vector<std::optional<Item>>(kInitialSlotCount);
    };
}
