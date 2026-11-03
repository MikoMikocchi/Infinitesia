#include "geology/catalog/MaterialLoader.h"

#include "geology/catalog/GeologyCatalog.h"
#include "geology/catalog/internal/CatalogJson.h"

#include <set>
namespace Infinitesia
{
    using namespace CatalogInternal;

    namespace
    {
        BlockClass MaterialClassField(const Json& object, const Where& where)
        {
            static constexpr std::pair<const char*, BlockClass> kNames[] = {
                { "air", BlockClass::Air }, { "fluid", BlockClass::Fluid },
                { "loose", BlockClass::Loose }, { "ice", BlockClass::Ice },
            };
            const std::string text = StringField(object, "class", where);
            for (const auto& [name, value] : kNames)
            {
                if (text == name)
                    return value;
            }
            Fail(where, "unknown material class '" + text + "' (rocks are read from rocks.json)");
        }
    }

    void LoadMaterials(const std::filesystem::path& path, std::vector<MaterialDef>& materials)
    {
        const Json root = ReadJson(path);
        Where where{ path.string(), "" };
        CheckSchemaVersion(root, where);
        std::size_t index = 0;
        for (const Json& entry : ArrayField(root, "materials", where))
        {
            Where record{ path.string(), "materials[" + std::to_string(index++) + "]" };
            MaterialDef material;
            material.key = StringField(entry, "id", record);
            record.record = material.key;
            const int numericId = IntField(entry, "numericId", record);
            material.blockClass = MaterialClassField(entry, record);
            const bool lowRange = material.blockClass == BlockClass::Air || material.blockClass == BlockClass::Fluid;
            const int lowest = lowRange ? 0 : kFirstLooseMaterial;
            const int highest = lowRange ? kFirstRockMaterial - 1 : kFirstReservedMaterial - 1;
            if (numericId < lowest || numericId > highest)
                Fail(record, "numericId is outside the range of its class");
            material.id = static_cast<MaterialId>(numericId);
            material.name = StringField(entry, "name", record);
            material.nameRu = entry.value("nameRu", std::string());
            material.density = NumberField(entry, "density", record);
            if (!(material.density > 0.0))
                Fail(record, "density must be positive");
            material.digClass = IntField(entry, "digClass", record);
            if (material.digClass < 0 || material.digClass > 5)
                Fail(record, "digClass must be 0-5");
            material.color = ColorField(entry, record);
            ProvenanceField(entry, record);
            materials.push_back(std::move(material));
        }
    }
}
