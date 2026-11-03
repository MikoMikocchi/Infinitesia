#include "world/SurfaceMaterials.h"

namespace Infinitesia
{
    namespace
    {
        MaterialId Find(const BlockRegistry& registry, const char* key)
        {
            const MaterialDef* material = registry.FindMaterial(key);
            if (material == nullptr)
                throw CatalogError(std::string("world: the block registry has no material '") + key + "'");
            return material->id;
        }
    }

    SurfaceMaterials SurfaceMaterials::Resolve(const BlockRegistry& registry)
    {
        SurfaceMaterials materials;
        materials.turf = Find(registry, "soil.turf");
        materials.subsoil = Find(registry, "soil.subsoil");
        materials.sand = Find(registry, "soil.sand");
        materials.bedrock = Find(registry, "rock.bedrock");
        materials.seawater = Find(registry, "fluid.seawater");
        return materials;
    }
}
