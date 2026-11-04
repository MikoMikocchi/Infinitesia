#include "domains/natural/geology/catalog/ColumnLoader.h"

#include "matter/CatalogJson.h"

namespace Infinitesia {
using namespace CatalogJson;

namespace {
BiosphereRequirement RequirementFromName(const std::string& text,
                                         const Where& where) {
  static constexpr std::pair<const char*, BiosphereRequirement> kNames[] = {
      {"freeOxygen", BiosphereRequirement::FreeOxygen},
      {"bandedIronFormations", BiosphereRequirement::BandedIronFormations},
      {"landPlants", BiosphereRequirement::LandPlants},
      {"organicRichMarineSediments",
       BiosphereRequirement::OrganicRichMarineSediments},
      {"calcareousPlankton", BiosphereRequirement::CalcareousPlankton},
  };
  for (const auto& [name, value] : kNames) {
    if (text == name) return value;
  }
  Fail(where, "unknown biosphere requirement '" + text + "'");
}
}  // namespace

void LoadColumns(const std::filesystem::path& path,
                 const MatterCatalog& materials,
                 std::array<ProvinceColumn, kProvinceTypeCount>& columns) {
  const Json root = ReadJson(path);
  Where where{path.string(), ""};
  const Json& table = Field(root, "columns", where);
  if (!table.is_object()) Fail(where, "field 'columns' must be an object");

  std::array<bool, kProvinceTypeCount> seen{};
  for (const auto& [name, definition] : table.items()) {
    Where record{path.string(), "columns." + name};
    int typeIndex = -1;
    for (int i = 0; i < kProvinceTypeCount; ++i) {
      if (ProvinceTypeName(static_cast<ProvinceType>(i)) == name) typeIndex = i;
    }
    if (typeIndex < 0) Fail(record, "unknown province type");
    const Json& layers = Field(definition, "layers", record);
    if (!layers.is_array() || layers.empty())
      Fail(record, "a column needs at least one layer");

    ProvinceColumn& column = columns[static_cast<std::size_t>(typeIndex)];
    column.type = static_cast<ProvinceType>(typeIndex);
    column.bottomUp.clear();

    const Json& structure = Field(definition, "structure", record);
    StructureStyle& style = column.style;
    ReadRange(structure, "dipDegrees", record, 0.0, 89.0, style.minDipDegrees,
              style.maxDipDegrees);
    ReadRange(structure, "layerVariability", record, 0.0, 2.0,
              style.minVariability, style.maxVariability);
    style.foldAmplitude = NumberField(structure, "foldAmplitude", record);
    if (!(style.foldAmplitude >= 0.0 && style.foldAmplitude <= 1.0))
      Fail(record, "foldAmplitude must be 0-1");
    style.foldCount = IntField(structure, "foldCount", record);
    if (style.foldCount < 0 || style.foldCount > 6)
      Fail(record, "foldCount must be 0-6");

    const Json& faults = Field(structure, "faults", record);
    FaultStyle& fault = style.faults;
    fault.spacingMeters = NumberField(faults, "spacingM", record);
    if (!(fault.spacingMeters >= 50.0 && fault.spacingMeters <= 20000.0))
      Fail(record, "fault spacingM must be 50-20000");
    fault.probability = NumberField(faults, "probability", record);
    if (!(fault.probability >= 0.0 && fault.probability <= 1.0))
      Fail(record, "fault probability must be 0-1");
    ReadRange(faults, "throwM", record, 0.0, 500.0, fault.minSlipMeters,
              fault.maxSlipMeters);
    fault.normalShare = NumberField(faults, "normalShare", record);
    fault.thrustShare = NumberField(faults, "thrustShare", record);
    if (!(fault.normalShare >= 0.0 && fault.thrustShare >= 0.0 &&
          fault.normalShare + fault.thrustShare <= 1.0 + 1e-9))
      Fail(record,
           "fault normalShare and thrustShare must be shares that sum to at "
           "most 1");
    for (const Json& layer : layers) {
      const std::string rockKey = StringField(layer, "rock", record);
      const MatterDef* rock = materials.Find(rockKey);
      if (rock == nullptr || rock->blockClass != BlockClass::Rock)
        Fail(record, "unknown rock '" + rockKey + "'");
      ColumnLayer parsed;
      parsed.rock = rock->id;
      parsed.thicknessMeters = NumberField(layer, "thicknessM", record);
      if (!(parsed.thicknessMeters > 0.0))
        Fail(record, "layer thickness must be positive");
      if (layer.contains("requires"))
        parsed.needs =
            RequirementFromName(StringField(layer, "requires", record), record);
      column.bottomUp.push_back(parsed);
    }
    if (column.bottomUp.front().needs != BiosphereRequirement::None)
      Fail(record, "the basement layer cannot depend on the biosphere");
    seen[static_cast<std::size_t>(typeIndex)] = true;
  }
  for (int i = 0; i < kProvinceTypeCount; ++i) {
    if (!seen[static_cast<std::size_t>(i)]) {
      Fail(where,
           "no column for province type '" +
               std::string(ProvinceTypeName(static_cast<ProvinceType>(i))) +
               "'");
    }
  }
}
}  // namespace Infinitesia
