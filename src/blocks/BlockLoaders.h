#pragma once

#include "blocks/ShapeDef.h"

#include <filesystem>
#include <vector>

namespace Infinitesia
{
    // Reads shapes.json. Throws CatalogError.
    void LoadShapes(const std::filesystem::path& path, std::vector<ShapeDef>& shapes);
}
