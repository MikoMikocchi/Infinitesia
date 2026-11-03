#pragma once

#include "geology/catalog/CatalogTypes.h"

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // Reads province_columns.json; layers refer to rocks through `rockByKey`. Throws CatalogError.
    void LoadColumns(const std::filesystem::path& path, const std::unordered_map<std::string, int>& rockByKey,
                     const std::vector<Rock>& rocks, std::array<ProvinceColumn, kProvinceTypeCount>& columns);
}
