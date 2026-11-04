#pragma once

#include <array>
#include <filesystem>

#include "domains/natural/geology/catalog/ProvinceColumn.h"
#include "materials/MaterialCatalog.h"

namespace Infinitesia {
// Reads province_columns.json; layers refer to rocks of `materials` by key.
// Throws CatalogError.
void LoadColumns(const std::filesystem::path& path,
                 const MaterialCatalog& materials,
                 std::array<ProvinceColumn, kProvinceTypeCount>& columns);
}  // namespace Infinitesia
