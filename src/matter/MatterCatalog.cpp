#include "matter/MatterCatalog.h"

#include "matter/MatterLoader.h"

namespace Infinitesia {
MatterCatalog MatterCatalog::LoadFromFile(const std::filesystem::path& path) {
  std::vector<MatterDef> materials;
  ChemistryByKey chemistry;
  ModalByKey modal;
  LoadMaterialFile(path, materials, chemistry, modal);
  return Assemble(std::move(materials), std::move(chemistry), std::move(modal));
}

MatterCatalog MatterCatalog::Assemble(std::vector<MatterDef> materials,
                                      ChemistryByKey chemistry,
                                      ModalByKey modal) {
  MatterCatalog catalog;
  if (materials.size() > static_cast<std::size_t>(kMatterCapacity)) {
    throw CatalogError("materials: more than " + std::to_string(kMatterCapacity) +
                       " records");
  }
  // The dense id is the record's place in the list (docs/DataDriven.md, 4.2):
  // the data names a material by its string id, the runtime by this number.
  for (std::size_t i = 0; i < materials.size(); ++i) {
    materials[i].id = static_cast<MatterId>(i);
    if (!catalog.m_byKey.emplace(materials[i].key, static_cast<MatterId>(i))
             .second) {
      throw CatalogError("materials: duplicate id '" + materials[i].key + "'");
    }
  }
  catalog.m_matter = std::move(materials);

  if (catalog.m_matter.size() <= kAirMatter ||
      catalog.m_matter[kAirMatter].blockClass != BlockClass::Air)
    throw CatalogError("materials: the first material must be air");
  if (catalog.m_matter.size() <= kWaterMatter ||
      catalog.m_matter[kWaterMatter].blockClass != BlockClass::Fluid)
    throw CatalogError("materials: the second material must be a fluid (water)");

  const auto indexOf = [&catalog](const std::string& key) -> int {
    const auto found = catalog.m_byKey.find(key);
    return found == catalog.m_byKey.end() ? -1
                                          : static_cast<int>(found->second);
  };

  for (auto& [key, info] : chemistry) {
    const int index = indexOf(key);
    if (index < 0)
      throw CatalogError("materials: chemistry given for unknown material '" +
                         key + "'");
    catalog.m_chemistry.emplace(static_cast<MatterId>(index), std::move(info));
  }
  for (auto& [key, pending] : modal) {
    const int index = indexOf(key);
    if (index < 0)
      throw CatalogError(
          "materials: modal composition given for unknown material '" + key +
          "'");
    ModalComposition composition;
    composition.rockClass = pending.rockClass;
    for (const auto& [mineralKey, fraction] : pending.parts) {
      const int mineralIndex = indexOf(mineralKey);
      if (mineralIndex < 0)
        throw CatalogError("materials: modal composition of '" + key +
                           "' names unknown mineral '" + mineralKey + "'");
      const MatterDef& mineral = catalog.m_matter[mineralIndex];
      if (mineral.blockClass != BlockClass::Mineral)
        throw CatalogError("materials: modal composition of '" + key +
                           "' names '" + mineralKey +
                           "', which is not a mineral");
      composition.parts.push_back({static_cast<MatterId>(mineralIndex),
                                   fraction});
    }
    catalog.m_modal.emplace(static_cast<MatterId>(index),
                            std::move(composition));
  }
  for (const auto& entry : catalog.m_modal) {
    for (const ModalEntry& part : entry.second.parts) {
      if (!catalog.m_chemistry.contains(part.mineral))
        throw CatalogError("materials: mineral '" +
                           catalog.m_matter[part.mineral].key +
                           "' has no chemistry");
    }
  }
  return catalog;
}

const MatterDef* MatterCatalog::Find(std::string_view key) const noexcept {
  const auto found = m_byKey.find(std::string(key));
  return found == m_byKey.end()
             ? nullptr
             : &m_matter[static_cast<std::size_t>(found->second)];
}

const MatterDef* MatterCatalog::Get(MatterId id) const noexcept {
  return id < m_matter.size() ? &m_matter[static_cast<std::size_t>(id)]
                              : nullptr;
}

const ChemistryInfo* MatterCatalog::ChemistryOf(
    MatterId id) const noexcept {
  const auto found = m_chemistry.find(id);
  return found == m_chemistry.end() ? nullptr : &found->second;
}

const ModalComposition* MatterCatalog::ModalOf(MatterId id) const noexcept {
  const auto found = m_modal.find(id);
  return found == m_modal.end() ? nullptr : &found->second;
}

RgbColor MatterCatalog::ColorOf(MatterId id) const noexcept {
  const MatterDef* material = Get(id);
  return material == nullptr ? RgbColor{255, 0, 255} : material->color;
}

Chemistry::ElementMassFractions MatterCatalog::ElementMassFractionsOf(
    MatterId id) const {
  if (const ChemistryInfo* chemistry = ChemistryOf(id))
    return chemistry->composition.MassFractions();
  const ModalComposition* rock = ModalOf(id);
  if (rock == nullptr || rock->parts.empty()) return {};
  std::vector<Chemistry::MassPart> parts;
  parts.reserve(rock->parts.size());
  for (const ModalEntry& entry : rock->parts) {
    const ChemistryInfo& mineral = *ChemistryOf(entry.mineral);
    parts.push_back({mineral.composition.MassFractions(),
                     entry.fraction * Get(entry.mineral)->density});
  }
  return Chemistry::Mix(parts);
}
}  // namespace Infinitesia
