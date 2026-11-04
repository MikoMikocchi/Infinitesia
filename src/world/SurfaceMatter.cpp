#include "world/SurfaceMatter.h"

namespace Infinitesia {
namespace {
MatterId Find(const BlockRegistry& registry, const char* key) {
  const MatterDef* material = registry.FindMatter(key);
  if (material == nullptr)
    throw CatalogError(
        std::string("world: the block registry has no material '") + key + "'");
  return material->id;
}
}  // namespace

SurfaceMatter SurfaceMatter::Resolve(const BlockRegistry& registry) {
  SurfaceMatter materials;
  materials.turf = Find(registry, "soil.turf");
  materials.subsoil = Find(registry, "soil.subsoil");
  materials.sand = Find(registry, "soil.sand");
  materials.bedrock = Find(registry, "rock.bedrock");
  materials.seawater = Find(registry, "fluid.seawater");
  return materials;
}
}  // namespace Infinitesia
