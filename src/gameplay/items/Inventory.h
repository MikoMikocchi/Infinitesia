#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "gameplay/items/Item.h"

namespace Infinitesia {
class Inventory {
 public:
  static constexpr std::size_t kInitialSlotCount = 4;

  const std::vector<std::optional<Item>>& GetSlots() const;
  void AddSlots(std::size_t count);
  bool CanStore(const Item& item) const;
  bool TryAdd(const Item& item);

 private:
  std::vector<std::optional<Item>> m_slots =
      std::vector<std::optional<Item>>(kInitialSlotCount);
};
}  // namespace Infinitesia
