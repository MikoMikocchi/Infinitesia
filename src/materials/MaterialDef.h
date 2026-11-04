#pragma once

#include <string>
#include <vector>

#include "domains/natural/chemistry/Composition.h"
#include "materials/MaterialId.h"
#include "materials/MaterialTypes.h"

namespace Infinitesia {
// The one record of everything a cell can be made of: air, a fluid, a rock,
// loose ground, a mineral, a built material (docs/06_BLOCKS.md, section 3.2).
// It holds what every material has; what only some have lives in side tables of
// the MaterialCatalog, found by the same id (ChemistryInfo, ModalComposition).
struct MaterialDef {
  MaterialId id = 0;
  std::string
      key;  // "rock.granite", "soil.sand", "fluid.water", "mineral.quartz"
  std::string name;
  BlockClass blockClass = BlockClass::Air;
  double density = 0.0;  // kg/m3
  int digClass = 0;      // 0 cannot be dug .. 5 very hard
  RgbColor color;
  Provenance provenance = Provenance::Gameplay;
  // Tags group materials so rules ask "is it water" instead of naming ids; the
  // class tag (ClassTag) is always present.
  std::vector<std::string> tags;
  LightRgb opacity{kMaxLightLevel, kMaxLightLevel,
                   kMaxLightLevel};  // dimming per channel
  LightRgb emission;  // light the material gives off, per channel
  // Rules a shape may narrow (docs/06_BLOCKS.md, section 3.2); air is neither.
  bool walkable = true;  // a worker can stand on it and walk through it
  bool diggable = true;  // a work order can remove it
};

// What a material is chemically; present for minerals.
struct ChemistryInfo {
  std::string formula;  // as written in the catalog
  Chemistry::Composition composition;
  double molarMass = 0.0;      // g/mol, from the composition
  double mohs = 0.0;           // Mohs hardness 0-10
  bool solidSolution = false;  // the formula is a typical member of a series
};

// Share of one mineral in a rock, by volume.
struct ModalEntry {
  MaterialId mineral = 0;
  double fraction = 0.0;
};

// What a rock is geologically: its class and the mixture of minerals it is made
// of; present for rocks. `parts` is empty when the composition is not filled in
// yet.
struct ModalComposition {
  RockClass rockClass = RockClass::IgneousIntrusive;
  std::vector<ModalEntry> parts;
};
}  // namespace Infinitesia
