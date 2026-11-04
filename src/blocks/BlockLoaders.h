#pragma once

#include <filesystem>
#include <vector>

#include "blocks/ShapeDef.h"

namespace Infinitesia {
// Reads shapes.json. Throws CatalogError.
void LoadShapes(const std::filesystem::path& path,
                std::vector<ShapeDef>& shapes);
}  // namespace Infinitesia
