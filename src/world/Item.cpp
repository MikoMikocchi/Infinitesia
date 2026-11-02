#include "world/Item.h"

#include <stdexcept>

namespace Infinitesia
{
    Item::Item(ItemType type, int quantity)
        : m_type(type),
          m_quantity(quantity)
    {
        if (quantity <= 0)
        {
            throw std::invalid_argument("Item quantity must be greater than zero.");
        }
    }

    ItemType Item::GetType() const
    {
        return m_type;
    }

    int Item::GetQuantity() const
    {
        return m_quantity;
    }
}
