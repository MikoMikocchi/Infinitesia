#pragma once

#include "gameplay/content/ContentTypes.h"

namespace Infinitesia
{
// A stack of one kind of item. The definition belongs to the GameContent, which
// must outlive every item.
class Item
{
public:
	Item(const ItemDef &def, int quantity);

	const ItemDef &GetDef() const;
	int GetQuantity() const;

private:
	const ItemDef *m_def;
	int m_quantity;
};
} // namespace Infinitesia
