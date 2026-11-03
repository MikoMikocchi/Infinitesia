#pragma once

#include "blocks/ShapeDef.h"
#include "geology/catalog/CatalogTypes.h"

#include <filesystem>
#include <vector>

namespace Infinitesia
{
    // Reads built_materials.json: the materials that are not geology (class Built). Throws CatalogError.
    void LoadBuiltMaterials(const std::filesystem::path& path, std::vector<MaterialDef>& materials);

    // Reads shapes.json. Throws CatalogError.
    void LoadShapes(const std::filesystem::path& path, std::vector<ShapeDef>& shapes);
}
