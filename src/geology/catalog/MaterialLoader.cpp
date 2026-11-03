#include "geology/catalog/MaterialLoader.h"

#include "geology/catalog/GeologyCatalog.h"
#include "geology/catalog/internal/CatalogJson.h"

#include <algorithm>
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

    namespace
    {
        std::uint8_t LightLevel(const Json& value, const char* key, const Where& where)
        {
            if (!value.is_number_integer())
                Fail(where, std::string("field '") + key + "' must hold whole numbers");
            const int level = value.get<int>();
            if (level < 0 || level > kMaxLightLevel)
                Fail(where, std::string("field '") + key + "' must be 0-15");
            return static_cast<std::uint8_t>(level);
        }

        // A light value is written as one number (all channels) or as [r, g, b]; absent gives the fallback.
        LightRgb LightField(const Json& object, const char* key, const Where& where, LightRgb fallback, bool allowNumber)
        {
            if (!object.contains(key))
                return fallback;
            const Json& value = object.at(key);
            if (allowNumber && value.is_number())
            {
                const std::uint8_t level = LightLevel(value, key, where);
                return { level, level, level };
            }
            if (!value.is_array() || value.size() != 3)
                Fail(where, std::string("field '") + key + "' must be " + (allowNumber ? "a number or " : "") + "[r, g, b]");
            return { LightLevel(value[0], key, where), LightLevel(value[1], key, where), LightLevel(value[2], key, where) };
        }

        std::vector<std::string> TagsField(const Json& object, BlockClass blockClass, const Where& where)
        {
            std::vector<std::string> tags{ ClassTag(blockClass) };
            if (!object.contains("tags"))
                return tags;
            const Json& list = object.at("tags");
            if (!list.is_array())
                Fail(where, "field 'tags' must be an array of strings");
            for (const Json& tag : list)
            {
                if (!tag.is_string() || tag.get<std::string>().empty())
                    Fail(where, "field 'tags' must be an array of non-empty strings");
                const std::string text = tag.get<std::string>();
                if (std::find(tags.begin(), tags.end(), text) == tags.end())
                    tags.push_back(text);
            }
            return tags;
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
            material.tags = TagsField(entry, material.blockClass, record);
            material.opacity = LightField(entry, "opacity", record, DefaultOpacity(material.blockClass), true);
            material.emission = LightField(entry, "emission", record, LightRgb{}, false);
            ProvenanceField(entry, record);
            materials.push_back(std::move(material));
        }
    }
}
