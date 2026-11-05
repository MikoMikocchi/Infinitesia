#include "gameplay/items/Inventory.h"

#include <algorithm>
#include <stdexcept>

namespace Infinitesia
{
const std::vector<std::optional<Item>> &Inventory::GetSlots() const
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

bool Inventory::CanStore(const Item &item) const
{
	int remaining = item.GetQuantity();
	for (const std::optional<Item> &slot : m_slots)
	{
		if (slot.has_value() && &slot->GetDef() == &item.GetDef())
		{
			remaining -= item.GetDef().maxStack - slot->GetQuantity();
			if (remaining <= 0)
			{
				return true;
			}
		}
	}

	for (const std::optional<Item> &slot : m_slots)
	{
		if (!slot.has_value())
		{
			remaining -= item.GetDef().maxStack;
			if (remaining <= 0)
			{
				return true;
			}
		}
	}
	return false;
}

bool Inventory::TryAdd(const Item &item)
{
	if (!CanStore(item))
	{
		return false;
	}

	int remaining = item.GetQuantity();
	for (std::optional<Item> &slot : m_slots)
	{
		if (slot.has_value() && &slot->GetDef() == &item.GetDef() && slot->GetQuantity() < item.GetDef().maxStack)
		{
			const int added = std::min(remaining, item.GetDef().maxStack - slot->GetQuantity());
			slot = Item(item.GetDef(), slot->GetQuantity() + added);
			remaining -= added;
			if (remaining == 0)
			{
				return true;
			}
		}
	}

	for (std::optional<Item> &slot : m_slots)
	{
		if (!slot.has_value())
		{
			const int added = std::min(remaining, item.GetDef().maxStack);
			slot = Item(item.GetDef(), added);
			remaining -= added;
			if (remaining == 0)
			{
				return true;
			}
		}
	}
	return false;
}
} // namespace Infinitesia
