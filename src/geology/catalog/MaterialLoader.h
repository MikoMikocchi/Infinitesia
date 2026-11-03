#pragma once

#include "geology/catalog/CatalogTypes.h"

#include <filesystem>
#include <vector>

namespace Infinitesia
{
    // Reads materials.json: air, fluids, loose ground and ice (rocks come from rocks.json). Throws CatalogError.
    void LoadMaterials(const std::filesystem::path& path, std::vector<MaterialDef>& materials);
}
