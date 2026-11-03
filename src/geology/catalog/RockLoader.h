#pragma once

#include "geology/catalog/CatalogTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // Reads rocks.json; modal compositions refer to minerals through `mineralByKey`. Throws CatalogError.
    void LoadRocks(const std::filesystem::path& path, const std::unordered_map<std::string, int>& mineralByKey,
                   std::vector<Rock>& rocks, std::unordered_map<std::string, int>& byKey);
}
