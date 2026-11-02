#pragma once

namespace Infinitesia
{
    enum class ItemType
    {
        Logs
    };

    class Item
    {
    public:
        Item(ItemType type, int quantity);

        ItemType GetType() const;
        int GetQuantity() const;

    private:
        ItemType m_type;
        int m_quantity;
    };
}
