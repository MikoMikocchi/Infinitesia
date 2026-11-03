#pragma once

#include "geology/catalog/CatalogTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // Reads minerals.json into `minerals`, indexing them by id in `byKey`. Throws CatalogError.
    void LoadMinerals(const std::filesystem::path& path, std::vector<Mineral>& minerals,
                      std::unordered_map<std::string, int>& byKey);
}
