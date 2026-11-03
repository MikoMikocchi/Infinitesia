#include "blocks/BlockRegistry.h"

#include "blocks/BlockLoaders.h"

#include <algorithm>
#include <set>

namespace Infinitesia
{
    namespace
    {
        MaterialDef MakeBuiltInMaterial(MaterialId id, const char* key, const char* name, BlockClass blockClass,
                                        double density, int digClass, RgbColor color,
                                        std::vector<std::string> extraTags = {})
        {
            MaterialDef material;
            material.id = id;
            material.key = key;
            material.name = name;
            material.blockClass = blockClass;
            material.density = density;
            material.digClass = digClass;
            material.color = color;
            material.tags.push_back(ClassTag(blockClass));
            for (std::string& tag : extraTags)
                material.tags.push_back(std::move(tag));
            material.opacity = DefaultOpacity(blockClass);
            material.walkable = material.diggable = blockClass != BlockClass::Air;
            return material;
        }
    }

    BlockRegistry BlockRegistry::Build(const GeologyCatalog& geology, const std::filesystem::path& directory)
    {
        std::vector<MaterialDef> materials(geology.Materials().begin(), geology.Materials().end());
        LoadBuiltMaterials(directory / "built_materials.json", materials);
        std::vector<ShapeDef> shapes;
        LoadShapes(directory / "shapes.json", shapes);
        return Assemble(std::move(materials), std::move(shapes));
    }

    BlockRegistry BlockRegistry::BuiltIn()
    {
        std::vector<MaterialDef> materials;
        materials.push_back(MakeBuiltInMaterial(kAirMaterial, "air", "air", BlockClass::Air, 1.2, 0, { 0, 0, 0 }));
        materials.push_back(MakeBuiltInMaterial(kWaterMaterial, "fluid.water", "water", BlockClass::Fluid, 1000.0, 0,
                                                { 0x34, 0x6e, 0xaa }, { "water" }));
        materials.push_back(MakeBuiltInMaterial(2, "fluid.seawater", "seawater", BlockClass::Fluid, 1000.0, 0,
                                                { 0x34, 0x6e, 0xaa }, { "water" }));
        materials.push_back(MakeBuiltInMaterial(130, "rock.bedrock", "bedrock", BlockClass::Rock, 2700.0, 4,
                                                { 0x84, 0x89, 0x8e }));
        materials.push_back(MakeBuiltInMaterial(513, "soil.subsoil", "subsoil", BlockClass::Loose, 1600.0, 1,
                                                { 0x7a, 0x5a, 0x3a }, { "soil" }));
        materials.push_back(MakeBuiltInMaterial(516, "soil.sand", "sand", BlockClass::Loose, 1600.0, 1,
                                                { 0xdb, 0xcb, 0x8c }, { "soil" }));
        materials.push_back(MakeBuiltInMaterial(527, "soil.turf", "turf", BlockClass::Loose, 1300.0, 1,
                                                { 0x52, 0x91, 0x42 }, { "soil" }));
        return Assemble(std::move(materials), { ShapeDef{} });
    }

    BlockRegistry BlockRegistry::Assemble(std::vector<MaterialDef> materials, std::vector<ShapeDef> shapes)
    {
        BlockRegistry registry;
        std::sort(materials.begin(), materials.end(),
                  [](const MaterialDef& a, const MaterialDef& b) { return a.id < b.id; });
        registry.m_materials = std::move(materials);
        registry.m_props.assign(kRegistryMaterialCapacity, MaterialProps{});
        registry.m_materialIndex.assign(kRegistryMaterialCapacity, -1);

        std::set<std::string> keys;
        for (std::size_t i = 0; i < registry.m_materials.size(); ++i)
        {
            const MaterialDef& material = registry.m_materials[i];
            if (material.id >= kRegistryMaterialCapacity)
                throw CatalogError("blocks: material '" + material.key + "' has an id above 4095");
            if (registry.m_materialIndex[material.id] >= 0)
            {
                throw CatalogError("blocks: numeric id " + std::to_string(material.id) + " is used by both '" +
                                   registry.m_materials[static_cast<std::size_t>(registry.m_materialIndex[material.id])].key +
                                   "' and '" + material.key + "'");
            }
            if (!keys.insert(material.key).second)
                throw CatalogError("blocks: duplicate material id '" + material.key + "'");
            registry.m_materialIndex[material.id] = static_cast<int>(i);
            registry.m_materialByKey.emplace(material.key, static_cast<int>(i));

            MaterialProps& props = registry.m_props[material.id];
            const bool occupied = material.blockClass != BlockClass::Air;
            props.flags = static_cast<std::uint8_t>((occupied ? kOccupied : 0) |
                                                    (occupied && material.walkable ? kWalkable : 0) |
                                                    (occupied && material.diggable ? kDiggable : 0));
            props.digClass = static_cast<std::uint8_t>(material.digClass);
            props.opacity = material.opacity;
            props.emission = material.emission;
            for (const std::string& name : material.tags)
            {
                if (const std::optional<Tag> tag = TagFromName(name))
                    props.tags |= 1u << static_cast<unsigned>(*tag);
            }
            if (material.emission != LightRgb{})
                props.tags |= 1u << static_cast<unsigned>(Tag::LightSource);
        }
        const MaterialDef* air = registry.GetMaterial(kAirMaterial);
        if (air == nullptr || air->blockClass != BlockClass::Air)
            throw CatalogError("blocks: material 0 must be air");

        std::sort(shapes.begin(), shapes.end(), [](const ShapeDef& a, const ShapeDef& b) { return a.id < b.id; });
        registry.m_shapes = std::move(shapes);
        std::set<std::string> shapeKeys;
        for (std::size_t i = 0; i < registry.m_shapes.size(); ++i)
        {
            const ShapeDef& shape = registry.m_shapes[i];
            if (registry.m_shapeKnown[shape.id])
                throw CatalogError("blocks: shape numeric id " + std::to_string(shape.id) + " is used twice");
            if (!shapeKeys.insert(shape.key).second)
                throw CatalogError("blocks: duplicate shape id '" + shape.key + "'");
            registry.m_shapeKnown[shape.id] = true;
            registry.m_shapeProps[shape.id] = { shape.faceMask, shape.walkable, shape.passesLight };
            registry.m_shapeByKey.emplace(shape.key, static_cast<int>(i));
        }
        if (!registry.m_shapeKnown[kShapeFull] || registry.m_shapes.front().key != "shape.full")
            throw CatalogError("blocks: shape 0 must be shape.full");
        return registry;
    }

    const MaterialDef* BlockRegistry::FindMaterial(std::string_view key) const noexcept
    {
        const auto found = m_materialByKey.find(std::string(key));
        return found == m_materialByKey.end() ? nullptr : &m_materials[static_cast<std::size_t>(found->second)];
    }

    const MaterialDef* BlockRegistry::GetMaterial(MaterialId id) const noexcept
    {
        if (id >= m_materialIndex.size() || m_materialIndex[id] < 0)
            return nullptr;
        return &m_materials[static_cast<std::size_t>(m_materialIndex[id])];
    }

    const ShapeDef* BlockRegistry::FindShape(std::string_view key) const noexcept
    {
        const auto found = m_shapeByKey.find(std::string(key));
        return found == m_shapeByKey.end() ? nullptr : &m_shapes[static_cast<std::size_t>(found->second)];
    }

    const ShapeDef* BlockRegistry::GetShape(ShapeId id) const noexcept
    {
        for (const ShapeDef& shape : m_shapes)
        {
            if (shape.id == id)
                return &shape;
        }
        return nullptr;
    }

    bool BlockRegistry::HasTag(MaterialId material, std::string_view tag) const noexcept
    {
        const MaterialDef* def = GetMaterial(material);
        return def != nullptr && std::find(def->tags.begin(), def->tags.end(), tag) != def->tags.end();
    }
}
