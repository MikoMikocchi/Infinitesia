#include "blocks/BlockLoaders.h"

#include "geology/catalog/GeologyCatalog.h"
#include "geology/catalog/internal/CatalogJson.h"

#include <set>

namespace Infinitesia
{
    using namespace CatalogInternal;

    namespace
    {
        std::uint8_t FacesField(const Json& entry, const Where& where)
        {
            const Json& faces = Field(entry, "faces", where);
            if (faces.is_string() && faces.get<std::string>() == "all")
                return kFaceAll;
            if (!faces.is_array())
                Fail(where, "field 'faces' must be \"all\" or an array of face names");

            static constexpr std::pair<const char*, std::uint8_t> kNames[] = {
                { "top", kFaceTop }, { "bottom", kFaceBottom }, { "north", kFaceNorth },
                { "south", kFaceSouth }, { "east", kFaceEast }, { "west", kFaceWest },
            };
            std::uint8_t mask = 0;
            for (const Json& face : faces)
            {
                bool known = false;
                for (const auto& [name, bit] : kNames)
                {
                    if (face.is_string() && face.get<std::string>() == name)
                    {
                        mask = static_cast<std::uint8_t>(mask | bit);
                        known = true;
                    }
                }
                if (!known)
                    Fail(where, "unknown face in 'faces' (top, bottom, north, south, east, west)");
            }
            return mask;
        }
    }

    void LoadBuiltMaterials(const std::filesystem::path& path, std::vector<MaterialDef>& materials)
    {
        const Json root = ReadJson(path);
        const Where where{ path.string(), "" };
        std::size_t index = 0;
        for (const Json& entry : ArrayField(root, "materials", where))
        {
            Where record{ path.string(), "materials[" + std::to_string(index++) + "]" };
            MaterialDef material;
            material.key = StringField(entry, "id", record);
            record.record = material.key;
            if (!StartsWith(material.key, "built."))
                Fail(record, "id must start with 'built.'");
            const int numericId = IntField(entry, "numericId", record);
            if (numericId < kFirstReservedMaterial || numericId >= 4096)
                Fail(record, "numericId must be in 1024-4095");
            material.id = static_cast<MaterialId>(numericId);
            material.blockClass = BlockClass::Built;
            material.name = StringField(entry, "name", record);
            material.density = NumberField(entry, "density", record);
            if (!(material.density > 0.0))
                Fail(record, "density must be positive");
            material.digClass = IntField(entry, "digClass", record);
            if (material.digClass < 0 || material.digClass > 5)
                Fail(record, "digClass must be 0-5");
            material.color = ColorField(entry, record);
            ProvenanceField(entry, record);
            material.tags = TagsField(entry, material.blockClass, record);
            material.opacity = LightField(entry, "opacity", record, DefaultOpacity(material.blockClass), true);
            material.emission = LightField(entry, "emission", record, LightRgb{}, false);
            material.walkable = BoolField(entry, "walkable", record, true);
            material.diggable = BoolField(entry, "diggable", record, true);
            materials.push_back(std::move(material));
        }
    }

    void LoadShapes(const std::filesystem::path& path, std::vector<ShapeDef>& shapes)
    {
        const Json root = ReadJson(path);
        const Where where{ path.string(), "" };
        std::size_t index = 0;
        for (const Json& entry : ArrayField(root, "shapes", where))
        {
            Where record{ path.string(), "shapes[" + std::to_string(index++) + "]" };
            ShapeDef shape;
            shape.key = StringField(entry, "id", record);
            record.record = shape.key;
            if (!StartsWith(shape.key, "shape."))
                Fail(record, "id must start with 'shape.'");
            const int numericId = IntField(entry, "numericId", record);
            if (numericId < 0 || numericId >= kShapeCapacity)
                Fail(record, "numericId must be in 0-255");
            shape.id = static_cast<ShapeId>(numericId);
            shape.faceMask = FacesField(entry, record);
            shape.walkable = BoolField(entry, "walkable", record, true);
            shape.passesLight = BoolField(entry, "passesLight", record, false);
            shape.mesh = StringField(entry, "mesh", record);
            if (shape.mesh != "cube")
                Fail(record, "mesh must be 'cube' (the only geometry that exists)");
            shapes.push_back(std::move(shape));
        }
    }
}
