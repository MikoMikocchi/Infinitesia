#include "gameplay/items/Item.h"

#include <stdexcept>

namespace Infinitesia
{
    Item::Item(const ItemDef& def, int quantity)
        : m_def(&def),
          m_quantity(quantity)
    {
        if (quantity <= 0)
        {
            throw std::invalid_argument("Item quantity must be greater than zero.");
        }
    }

    const ItemDef& Item::GetDef() const
    {
        return *m_def;
    }

    int Item::GetQuantity() const
    {
        return m_quantity;
    }
}
