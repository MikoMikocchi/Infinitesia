#pragma once

#include <array>
#include <filesystem>

#include "domains/natural/geology/catalog/ProvinceColumn.h"
#include "matter/MatterCatalog.h"

namespace Infinitesia {
// Reads province_columns.json; layers refer to rocks of `materials` by key.
// Throws CatalogError.
void LoadColumns(const std::filesystem::path& path,
                 const MatterCatalog& materials,
                 std::array<ProvinceColumn, kProvinceTypeCount>& columns);
}  // namespace Infinitesia
