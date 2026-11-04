#pragma once

#include <filesystem>
#include <vector>

#include "materials/MaterialCatalog.h"

namespace Infinitesia {
// Reads materials.json into the three tables. Throws CatalogError.
void LoadMaterialFile(const std::filesystem::path& path,
                      std::vector<MaterialDef>& materials,
                      MaterialCatalog::ChemistryTable& chemistry,
                      MaterialCatalog::ModalTable& modal);
}  // namespace Infinitesia
