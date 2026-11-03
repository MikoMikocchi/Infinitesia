#include "gameplay/items/Inventory.h"

#include <algorithm>
#include <stdexcept>

namespace Infinitesia
{
    const std::vector<std::optional<Item>>& Inventory::GetSlots() const
    {
        return m_slots;
    }

    void Inventory::AddSlots(std::size_t count)
    {
        if (count > m_slots.max_size() - m_slots.size())
        {
            throw std::length_error("Inventory slot capacity is too large.");
        }
        m_slots.resize(m_slots.size() + count);
    }

    bool Inventory::CanStore(const Item& item) const
    {
        int remaining = item.GetQuantity();
        for (const std::optional<Item>& slot : m_slots)
        {
            if (slot.has_value() && slot->GetType() == item.GetType())
            {
                remaining -= kMaxStackSize - slot->GetQuantity();
                if (remaining <= 0)
                {
                    return true;
                }
            }
        }

        for (const std::optional<Item>& slot : m_slots)
        {
            if (!slot.has_value())
            {
                remaining -= kMaxStackSize;
                if (remaining <= 0)
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool Inventory::TryAdd(const Item& item)
    {
        if (!CanStore(item))
        {
            return false;
        }

        int remaining = item.GetQuantity();
        for (std::optional<Item>& slot : m_slots)
        {
            if (slot.has_value() && slot->GetType() == item.GetType() &&
                slot->GetQuantity() < kMaxStackSize)
            {
                const int added = std::min(
                    remaining,
                    kMaxStackSize - slot->GetQuantity());
                slot = Item(item.GetType(), slot->GetQuantity() + added);
                remaining -= added;
                if (remaining == 0)
                {
                    return true;
                }
            }
        }

        for (std::optional<Item>& slot : m_slots)
        {
            if (!slot.has_value())
            {
                const int added = std::min(remaining, kMaxStackSize);
                slot = Item(item.GetType(), added);
                remaining -= added;
                if (remaining == 0)
                {
                    return true;
                }
            }
        }
        return false;
    }
}
