#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domains/natural/chemistry/Composition.h"
#include "matter/CatalogError.h"
#include "matter/MatterDef.h"

namespace Infinitesia {
// The one list of every material of the game, loaded from matter.xml and
// validated as a whole (docs/06_BLOCKS.md, section 3). Each material is one
// `MatterDef`; the parts only some have are side tables found by the same
// MatterId: chemistry (minerals) and modal composition (rocks). The catalog
// gives each material its dense MatterId while loading (docs/DataDriven.md,
// section 4.2); the data names materials by their string id and never writes a
// number. Immutable after loading, so it can be shared freely between threads.
class MatterCatalog {
 public:
  // A rock's modal composition as the data writes it: the parts still name
  // minerals by their string id, which the catalog resolves while assembling.
  struct PendingModal {
    RockClass rockClass = RockClass::IgneousIntrusive;
    std::vector<std::pair<std::string, double>> parts;  // mineral id, volume
  };

  // While loading, the side tables are keyed by the material's string id; the
  // catalog turns them into tables by MatterId.
  using ChemistryByKey = std::unordered_map<std::string, ChemistryInfo>;
  using ModalByKey = std::unordered_map<std::string, PendingModal>;
  using ChemistryTable = std::unordered_map<MatterId, ChemistryInfo>;
  using ModalTable = std::unordered_map<MatterId, ModalComposition>;

  // Reads matter.xml. Throws CatalogError.
  static MatterCatalog LoadFromFile(const std::filesystem::path& path);

  // Gives every material its dense MatterId (its place in `materials`), resolves
  // the side tables into tables by MatterId and checks the whole catalog:
  // unique ids, air first and a fluid second, and modal parts that name
  // minerals which carry chemistry.
  static MatterCatalog Assemble(std::vector<MatterDef> materials,
                                ChemistryByKey chemistry, ModalByKey modal);

  // Every material, in id order.
  std::span<const MatterDef> Matter() const noexcept { return m_matter; }

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
  std::unordered_map<std::string, MatterId> m_byKey;
};
}  // namespace Infinitesia
