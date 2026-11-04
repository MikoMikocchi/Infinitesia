#include "materials/MaterialLoader.h"

#include <cmath>
#include <set>
#include <unordered_map>

#include "materials/CatalogJson.h"

namespace Infinitesia {
using namespace CatalogJson;

namespace {
BlockClass MaterialClassField(const Json& object, const Where& where) {
  static constexpr std::pair<const char*, BlockClass> kNames[] = {
      {"air", BlockClass::Air},         {"fluid", BlockClass::Fluid},
      {"loose", BlockClass::Loose},     {"ice", BlockClass::Ice},
      {"rock", BlockClass::Rock},       {"built", BlockClass::Built},
      {"mineral", BlockClass::Mineral}, {"plant", BlockClass::Plant},
  };
  const std::string text = StringField(object, "class", where);
  for (const auto& [name, value] : kNames) {
    if (text == name) return value;
  }
  Fail(where, "unknown material class '" + text + "'");
}

// The numeric ids a class may use (MaterialId.h) and the prefix its key must
// have.
struct ClassRule {
  int lowest;
  int highest;
  const char* prefix;  // nullptr: no rule
  const char* label;
};

ClassRule RuleOf(BlockClass blockClass) {
  switch (blockClass) {
    case BlockClass::Air:
    case BlockClass::Fluid:
      return {0, kFirstRockMaterial - 1, nullptr, "0-31"};
    case BlockClass::Rock:
      return {kFirstRockMaterial, kFirstLooseMaterial - 1, "rock.", "32-511"};
    case BlockClass::Loose:
    case BlockClass::Ice:
      return {kFirstLooseMaterial, kFirstBuiltMaterial - 1, nullptr,
              "512-1023"};
    case BlockClass::Built:
      return {kFirstBuiltMaterial, kFirstMineralMaterial - 1, "built.",
              "1024-2047"};
    case BlockClass::Mineral:
      return {kFirstMineralMaterial, kFirstPlantMaterial - 1, "mineral.",
              "2048-4095"};
    case BlockClass::Plant:
      return {kFirstPlantMaterial, kMaterialCapacity - 1, "plant.",
              "4096-5119"};
  }
  return {0, kMaterialCapacity - 1, nullptr, "0-4095"};
}

RockClass RockClassField(const Json& object, const Where& where) {
  static constexpr std::pair<const char*, RockClass> kNames[] = {
      {"igneous_intrusive", RockClass::IgneousIntrusive},
      {"igneous_extrusive", RockClass::IgneousExtrusive},
      {"volcaniclastic", RockClass::Volcaniclastic},
      {"sedimentary_clastic", RockClass::SedimentaryClastic},
      {"sedimentary_chemical", RockClass::SedimentaryChemical},
      {"sedimentary_biogenic", RockClass::SedimentaryBiogenic},
      {"metamorphic", RockClass::Metamorphic},
  };
  const std::string text = StringField(object, "class", where);
  for (const auto& [name, value] : kNames) {
    if (text == name) return value;
  }
  Fail(where, "unknown rock class '" + text + "'");
}

ChemistryInfo ReadChemistry(const Json& object, const Where& where) {
  ChemistryInfo chemistry;
  chemistry.formula = StringField(object, "formula", where);
  std::string error;
  if (!Chemistry::Composition::TryParse(chemistry.formula,
                                        chemistry.composition, &error))
    Fail(where, error);
  chemistry.molarMass = chemistry.composition.MolarMass();
  chemistry.mohs = NumberField(object, "mohs", where);
  if (!(chemistry.mohs >= 0.0 && chemistry.mohs <= 10.0))
    Fail(where, "Mohs hardness must be 0-10");
  chemistry.solidSolution = BoolField(object, "solidSolution", where, false);
  return chemistry;
}

// The modal composition names minerals by key, so it is read once every key is
// known.
ModalComposition ReadModal(
    const Json& object, const Where& where,
    const std::unordered_map<std::string, MaterialId>& idByKey) {
  ModalComposition modal;
  modal.rockClass = RockClassField(object, where);
  if (!object.contains("modal")) return modal;
  double sum = 0.0;
  std::set<MaterialId> used;
  for (const Json& part : ArrayField(object, "modal", where)) {
    const std::string mineralKey = StringField(part, "mineral", where);
    const auto found = idByKey.find(mineralKey);
    if (found == idByKey.end())
      Fail(where,
           "modal composition names unknown mineral '" + mineralKey + "'");
    if (!used.insert(found->second).second)
      Fail(where, "modal composition lists '" + mineralKey + "' twice");
    const double fraction = NumberField(part, "fraction", where);
    if (!(fraction > 0.0 && fraction <= 1.0))
      Fail(where, "modal fraction of '" + mineralKey + "' must be in (0, 1]");
    sum += fraction;
    modal.parts.push_back({found->second, fraction});
  }
  if (std::abs(sum - 1.0) > 1.0e-3)
    Fail(where, "modal fractions sum to " + std::to_string(sum) + ", not 1");
  return modal;
}
}  // namespace

void LoadMaterialFile(const std::filesystem::path& path,
                      std::vector<MaterialDef>& materials,
                      MaterialCatalog::ChemistryTable& chemistry,
                      MaterialCatalog::ModalTable& modal) {
  const Json root = ReadJson(path);
  const Where where{path.string(), ""};

  struct PendingRock {
    MaterialId id;
    const Json* object;
    Where record;
  };
  std::vector<PendingRock> rocks;
  std::unordered_map<std::string, MaterialId> idByKey;

  std::size_t index = 0;
  for (const Json& entry : ArrayField(root, "materials", where)) {
    Where record{path.string(), "materials[" + std::to_string(index++) + "]"};
    MaterialDef material;
    material.key = StringField(entry, "id", record);
    record.record = material.key;
    material.blockClass = MaterialClassField(entry, record);

    const ClassRule rule = RuleOf(material.blockClass);
    if (rule.prefix != nullptr && !StartsWith(material.key, rule.prefix))
      Fail(record, std::string("id must start with '") + rule.prefix + "'");
    const int numericId = IntField(entry, "numericId", record);
    if (numericId < rule.lowest || numericId > rule.highest)
      Fail(record,
           std::string("numericId is outside the range of its class (") +
               rule.label + ")");
    material.id = static_cast<MaterialId>(numericId);
    if (!idByKey.emplace(material.key, material.id).second)
      Fail(record, "duplicate id");

    material.name = StringField(entry, "name", record);
    material.density = NumberField(entry, "density", record);
    if (material.blockClass == BlockClass::Rock) {
      if (!(material.density >= 500.0 && material.density <= 6000.0))
        Fail(record, "density must be 500-6000 kg/m3");
    } else if (material.blockClass == BlockClass::Mineral) {
      if (!(material.density >= 500.0 && material.density <= 25000.0))
        Fail(record, "density must be 500-25000 kg/m3");
    } else if (!(material.density > 0.0)) {
      Fail(record, "density must be positive");
    }
    material.digClass = IntField(entry, "digClass", record);
    const bool needsDigClass = material.blockClass == BlockClass::Rock ||
                               material.blockClass == BlockClass::Mineral;
    if (material.digClass < (needsDigClass ? 1 : 0) || material.digClass > 5)
      Fail(record,
           needsDigClass ? "digClass must be 1-5" : "digClass must be 0-5");
    material.color = ColorField(entry, record);
    material.provenance = ProvenanceField(entry, record);
    material.tags = TagsField(entry, material.blockClass, record);
    material.opacity = LightField(entry, "opacity", record,
                                  DefaultOpacity(material.blockClass), true);
    material.emission =
        LightField(entry, "emission", record, LightRgb{}, false);
    const bool solid = material.blockClass != BlockClass::Air;
    material.walkable = BoolField(entry, "walkable", record, solid);
    material.diggable = BoolField(entry, "diggable", record, solid);

    if (material.blockClass == BlockClass::Mineral &&
        !entry.contains("chemistry"))
      Fail(record, "a mineral needs a 'chemistry' part");
    if (entry.contains("chemistry")) {
      const Json& part = entry.at("chemistry");
      if (!part.is_object())
        Fail(record, "field 'chemistry' must be an object");
      chemistry.emplace(material.id, ReadChemistry(part, record));
    }
    if (material.blockClass == BlockClass::Rock && !entry.contains("rock"))
      Fail(record, "a rock needs a 'rock' part");
    if (entry.contains("rock")) {
      if (material.blockClass != BlockClass::Rock)
        Fail(record, "only a rock may have a 'rock' part");
      if (!entry.at("rock").is_object())
        Fail(record, "field 'rock' must be an object");
      rocks.push_back({material.id, &entry.at("rock"), record});
    }
    materials.push_back(std::move(material));
  }

  for (const PendingRock& rock : rocks)
    modal.emplace(rock.id, ReadModal(*rock.object, rock.record, idByKey));
}
}  // namespace Infinitesia
