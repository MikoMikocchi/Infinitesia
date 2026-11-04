#pragma once

#include <filesystem>
#include <vector>

#include "matter/MatterCatalog.h"

namespace Infinitesia {
// Reads matter.json into the three tables. Throws CatalogError.
void LoadMaterialFile(const std::filesystem::path& path,
                      std::vector<MatterDef>& materials,
                      MatterCatalog::ChemistryTable& chemistry,
                      MatterCatalog::ModalTable& modal);
}  // namespace Infinitesia
