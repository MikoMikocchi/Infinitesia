#pragma once

#include <filesystem>
#include <vector>

#include "matter/MatterCatalog.h"
#include "matter/MatterDef.h"

namespace Infinitesia
{
// Reads matter.xml into the material list and its side tables (docs/
// DataDriven.md, section 4.6: parsing and validation; the catalog then gives
// the records their MatterId and resolves the references). A material keeps the
// string id of the data. Throws CatalogError.
void LoadMaterialFile(const std::filesystem::path &path, std::vector<MatterDef> &materials,
                      MatterCatalog::ChemistryByKey &chemistry, MatterCatalog::ModalByKey &modal);
} // namespace Infinitesia
