#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "domains/natural/chemistry/Composition.h"
#include "matter/CatalogError.h"
#include "matter/MatterDef.h"

namespace Infinitesia {
// The one list of every material of the game, loaded from matter.json and
// validated as a whole (docs/06_BLOCKS.md, section 3). Each material is one
// `MatterDef`; the parts only some have are side tables found by the same id:
// chemistry (minerals) and modal composition (rocks). Immutable after loading,
// so it can be shared freely between threads.
class MatterCatalog {
 public:
  using ChemistryTable = std::unordered_map<MatterId, ChemistryInfo>;
  using ModalTable = std::unordered_map<MatterId, ModalComposition>;

  // Reads matter.json. Throws CatalogError.
  static MatterCatalog LoadFromFile(const std::filesystem::path& path);

  // Checks and indexes materials already in memory: unique ids and keys, air at
  // 0, a fluid at 1, side tables that belong to listed materials, modal parts
  // that name minerals.
  static MatterCatalog Assemble(std::vector<MatterDef> materials,
                                  ChemistryTable chemistry, ModalTable modal);

  // Every material, sorted by id.
  std::span<const MatterDef> Matter() const noexcept {
    return m_matter;
  }

  const MatterDef* Find(std::string_view key) const noexcept;
  // nullptr for an id that is not in the catalog (including
  // kUnspecifiedMatter).
  const MatterDef* Get(MatterId id) const noexcept;

  // nullptr when the material has no such part.
  const ChemistryInfo* ChemistryOf(MatterId id) const noexcept;
  const ModalComposition* ModalOf(MatterId id) const noexcept;

  // Colour of a material; magenta for an unknown id so mistakes are visible.
  RgbColor ColorOf(MatterId id) const noexcept;

  // Element mass fractions of a mineral, or of a rock from its minerals (the
  // modal composition is by volume, so each mineral is weighted by its
  // density). Empty for anything else, and for a rock without a modal
  // composition.
  Chemistry::ElementMassFractions ElementMassFractionsOf(MatterId id) const;

 private:
  std::vector<MatterDef> m_matter;
  ChemistryTable m_chemistry;
  ModalTable m_modal;
  std::unordered_map<std::string, int> m_byKey;
  std::vector<int>
      m_index;  // MatterId -> index in m_matter, -1 when absent
};
}  // namespace Infinitesia
