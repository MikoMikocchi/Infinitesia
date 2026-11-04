#include "materials/MaterialCatalog.h"

#include <algorithm>
#include <set>

#include "materials/MaterialLoader.h"

namespace Infinitesia {
MaterialCatalog MaterialCatalog::LoadFromFile(
    const std::filesystem::path& path) {
  std::vector<MaterialDef> materials;
  ChemistryTable chemistry;
  ModalTable modal;
  LoadMaterialFile(path, materials, chemistry, modal);
  return Assemble(std::move(materials), std::move(chemistry), std::move(modal));
}

MaterialCatalog MaterialCatalog::Assemble(std::vector<MaterialDef> materials,
                                          ChemistryTable chemistry,
                                          ModalTable modal) {
  MaterialCatalog catalog;
  std::sort(
      materials.begin(), materials.end(),
      [](const MaterialDef& a, const MaterialDef& b) { return a.id < b.id; });
  catalog.m_materials = std::move(materials);
  catalog.m_chemistry = std::move(chemistry);
  catalog.m_modal = std::move(modal);

  catalog.m_index.assign(kMaterialCapacity, -1);
  std::set<std::string> keys;
  for (std::size_t i = 0; i < catalog.m_materials.size(); ++i) {
    const MaterialDef& material = catalog.m_materials[i];
    if (material.id >= kMaterialCapacity)
      throw CatalogError("materials: '" + material.key + "' has an id above " +
                         std::to_string(kMaterialCapacity - 1));
    if (catalog.m_index[material.id] >= 0) {
      throw CatalogError("materials: numeric id " +
                         std::to_string(material.id) + " is used by both '" +
                         catalog
                             .m_materials[static_cast<std::size_t>(
                                 catalog.m_index[material.id])]
                             .key +
                         "' and '" + material.key + "'");
    }
    if (!keys.insert(material.key).second)
      throw CatalogError("materials: duplicate id '" + material.key + "'");
    catalog.m_index[material.id] = static_cast<int>(i);
    catalog.m_byKey.emplace(material.key, static_cast<int>(i));
  }
  if (catalog.Get(kAirMaterial) == nullptr ||
      catalog.Get(kAirMaterial)->blockClass != BlockClass::Air)
    throw CatalogError("materials: material 0 must be air");
  if (catalog.Get(kWaterMaterial) == nullptr ||
      catalog.Get(kWaterMaterial)->blockClass != BlockClass::Fluid)
    throw CatalogError("materials: material 1 must be a fluid (water)");

  for (const auto& [id, info] : catalog.m_chemistry) {
    if (catalog.Get(id) == nullptr)
      throw CatalogError("materials: chemistry given for unknown material " +
                         std::to_string(id));
  }
  for (const auto& [id, composition] : catalog.m_modal) {
    const MaterialDef* rock = catalog.Get(id);
    if (rock == nullptr)
      throw CatalogError(
          "materials: modal composition given for unknown material " +
          std::to_string(id));
    for (const ModalEntry& part : composition.parts) {
      const MaterialDef* mineral = catalog.Get(part.mineral);
      if (mineral == nullptr || mineral->blockClass != BlockClass::Mineral) {
        throw CatalogError("materials: modal composition of '" + rock->key +
                           "' names material " + std::to_string(part.mineral) +
                           ", which is not a mineral");
      }
      if (catalog.ChemistryOf(part.mineral) == nullptr)
        throw CatalogError("materials: mineral '" + mineral->key +
                           "' has no chemistry");
    }
  }
  return catalog;
}

const MaterialDef* MaterialCatalog::Find(std::string_view key) const noexcept {
  const auto found = m_byKey.find(std::string(key));
  return found == m_byKey.end()
             ? nullptr
             : &m_materials[static_cast<std::size_t>(found->second)];
}

const MaterialDef* MaterialCatalog::Get(MaterialId id) const noexcept {
  if (id >= m_index.size() || m_index[id] < 0) return nullptr;
  return &m_materials[static_cast<std::size_t>(m_index[id])];
}

const ChemistryInfo* MaterialCatalog::ChemistryOf(
    MaterialId id) const noexcept {
  const auto found = m_chemistry.find(id);
  return found == m_chemistry.end() ? nullptr : &found->second;
}

const ModalComposition* MaterialCatalog::ModalOf(MaterialId id) const noexcept {
  const auto found = m_modal.find(id);
  return found == m_modal.end() ? nullptr : &found->second;
}

RgbColor MaterialCatalog::ColorOf(MaterialId id) const noexcept {
  const MaterialDef* material = Get(id);
  return material == nullptr ? RgbColor{255, 0, 255} : material->color;
}

Chemistry::ElementMassFractions MaterialCatalog::ElementMassFractionsOf(
    MaterialId id) const {
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
