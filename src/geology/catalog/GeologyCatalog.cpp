#include "geology/catalog/GeologyCatalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <set>

namespace Infinitesia
{
    namespace
    {
        using Json = nlohmann::json;

        // Context of an error message: the file and the record being read.
        struct Where
        {
            std::string file;
            std::string record;
        };

        [[noreturn]] void Fail(const Where& where, const std::string& what)
        {
            throw CatalogError(where.file + (where.record.empty() ? "" : ": " + where.record) + ": " + what);
        }

        Json ReadJson(const std::filesystem::path& path)
        {
            std::ifstream stream(path);
            if (!stream)
            {
                throw CatalogError(path.string() + ": cannot open the file");
            }
            try
            {
                return Json::parse(stream);
            }
            catch (const Json::parse_error& error)
            {
                throw CatalogError(path.string() + ": invalid JSON: " + error.what());
            }
        }

        const Json& Field(const Json& object, const char* key, const Where& where)
        {
            if (!object.is_object() || !object.contains(key))
            {
                Fail(where, std::string("missing field '") + key + "'");
            }
            return object.at(key);
        }

        std::string StringField(const Json& object, const char* key, const Where& where)
        {
            const Json& value = Field(object, key, where);
            if (!value.is_string())
            {
                Fail(where, std::string("field '") + key + "' must be a string");
            }
            return value.get<std::string>();
        }

        double NumberField(const Json& object, const char* key, const Where& where)
        {
            const Json& value = Field(object, key, where);
            if (!value.is_number())
            {
                Fail(where, std::string("field '") + key + "' must be a number");
            }
            return value.get<double>();
        }

        int IntField(const Json& object, const char* key, const Where& where)
        {
            const Json& value = Field(object, key, where);
            if (!value.is_number_integer())
            {
                Fail(where, std::string("field '") + key + "' must be an integer");
            }
            return value.get<int>();
        }

        const Json& ArrayField(const Json& object, const char* key, const Where& where)
        {
            const Json& value = Field(object, key, where);
            if (!value.is_array())
            {
                Fail(where, std::string("field '") + key + "' must be an array");
            }
            return value;
        }

        void CheckSchemaVersion(const Json& root, const Where& where, int expected = 1)
        {
            if (IntField(root, "schemaVersion", where) != expected)
            {
                Fail(where, "unsupported schemaVersion (this build reads version " + std::to_string(expected) + ")");
            }
        }

        void ReadRange(const Json& object, const char* key, const Where& where, double lowest, double highest,
                       double& first, double& second)
        {
            const Json& value = Field(object, key, where);
            if (!value.is_array() || value.size() != 2 || !value[0].is_number() || !value[1].is_number())
                Fail(where, std::string("field '") + key + "' must be [min, max]");
            first = value[0].get<double>();
            second = value[1].get<double>();
            if (!(first >= lowest && first <= second && second <= highest))
                Fail(where, std::string("field '") + key + "' must satisfy " + std::to_string(lowest) +
                            " <= min <= max <= " + std::to_string(highest));
        }

        bool StartsWith(const std::string& text, const char* prefix)
        {
            return text.rfind(prefix, 0) == 0;
        }

        int HexDigit(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }

        RgbColor ColorField(const Json& object, const Where& where)
        {
            const std::string text = StringField(object, "color", where);
            if (text.size() != 7 || text[0] != '#')
            {
                Fail(where, "colour must look like #rrggbb, got '" + text + "'");
            }
            int channel[3];
            for (int i = 0; i < 3; ++i)
            {
                const int high = HexDigit(text[static_cast<std::size_t>(1 + 2 * i)]);
                const int low = HexDigit(text[static_cast<std::size_t>(2 + 2 * i)]);
                if (high < 0 || low < 0)
                {
                    Fail(where, "colour must look like #rrggbb, got '" + text + "'");
                }
                channel[i] = high * 16 + low;
            }
            return { static_cast<std::uint8_t>(channel[0]), static_cast<std::uint8_t>(channel[1]),
                     static_cast<std::uint8_t>(channel[2]) };
        }

        Provenance ProvenanceField(const Json& object, const Where& where)
        {
            const std::string text = StringField(object, "provenance", where);
            if (text == "reference") return Provenance::Reference;
            if (text == "derived") return Provenance::Derived;
            if (text == "gameplay") return Provenance::Gameplay;
            Fail(where, "provenance must be reference, derived or gameplay, got '" + text + "'");
        }

        RockClass RockClassField(const Json& object, const Where& where)
        {
            static constexpr std::pair<const char*, RockClass> kNames[] = {
                { "igneous_intrusive", RockClass::IgneousIntrusive },
                { "igneous_extrusive", RockClass::IgneousExtrusive },
                { "volcaniclastic", RockClass::Volcaniclastic },
                { "sedimentary_clastic", RockClass::SedimentaryClastic },
                { "sedimentary_chemical", RockClass::SedimentaryChemical },
                { "sedimentary_biogenic", RockClass::SedimentaryBiogenic },
                { "metamorphic", RockClass::Metamorphic },
            };
            const std::string text = StringField(object, "class", where);
            for (const auto& [name, value] : kNames)
            {
                if (text == name)
                    return value;
            }
            Fail(where, "unknown rock class '" + text + "'");
        }

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

        BiosphereRequirement RequirementFromName(const std::string& text, const Where& where)
        {
            static constexpr std::pair<const char*, BiosphereRequirement> kNames[] = {
                { "freeOxygen", BiosphereRequirement::FreeOxygen },
                { "bandedIronFormations", BiosphereRequirement::BandedIronFormations },
                { "landPlants", BiosphereRequirement::LandPlants },
                { "organicRichMarineSediments", BiosphereRequirement::OrganicRichMarineSediments },
                { "calcareousPlankton", BiosphereRequirement::CalcareousPlankton },
            };
            for (const auto& [name, value] : kNames)
            {
                if (text == name)
                    return value;
            }
            Fail(where, "unknown biosphere requirement '" + text + "'");
        }

        void LoadMinerals(const std::filesystem::path& path, std::vector<Mineral>& minerals,
                          std::unordered_map<std::string, int>& byKey)
        {
            const Json root = ReadJson(path);
            Where where{ path.string(), "" };
            CheckSchemaVersion(root, where);
            std::set<int> numericIds;
            for (const Json& entry : ArrayField(root, "minerals", where))
            {
                Where record{ path.string(), "minerals[" + std::to_string(minerals.size()) + "]" };
                Mineral mineral;
                mineral.id = StringField(entry, "id", record);
                record.record = mineral.id;
                if (!StartsWith(mineral.id, "mineral."))
                    Fail(record, "id must start with 'mineral.'");
                if (!byKey.emplace(mineral.id, static_cast<int>(minerals.size())).second)
                    Fail(record, "duplicate id");
                mineral.numericId = IntField(entry, "numericId", record);
                if (mineral.numericId <= 0 || !numericIds.insert(mineral.numericId).second)
                    Fail(record, "numericId must be positive and unique");
                mineral.name = StringField(entry, "name", record);
                mineral.formula = StringField(entry, "formula", record);
                std::string error;
                if (!Chemistry::Composition::TryParse(mineral.formula, mineral.composition, &error))
                    Fail(record, error);
                mineral.molarMass = mineral.composition.MolarMass();
                mineral.density = NumberField(entry, "density", record);
                if (!(mineral.density >= 0.5 && mineral.density <= 25.0))
                    Fail(record, "density must be 0.5-25 g/cm3");
                mineral.mohs = NumberField(entry, "mohs", record);
                if (!(mineral.mohs >= 0.0 && mineral.mohs <= 10.0))
                    Fail(record, "Mohs hardness must be 0-10");
                mineral.color = ColorField(entry, record);
                mineral.provenance = ProvenanceField(entry, record);
                mineral.solidSolution = entry.value("solidSolution", false);
                minerals.push_back(std::move(mineral));
            }
        }

        void LoadRocks(const std::filesystem::path& path, const std::vector<Mineral>& minerals,
                       const std::unordered_map<std::string, int>& mineralByKey, std::vector<Rock>& rocks,
                       std::unordered_map<std::string, int>& byKey)
        {
            const Json root = ReadJson(path);
            Where where{ path.string(), "" };
            CheckSchemaVersion(root, where);
            std::set<int> numericIds;
            for (const Json& entry : ArrayField(root, "rocks", where))
            {
                Where record{ path.string(), "rocks[" + std::to_string(rocks.size()) + "]" };
                Rock rock;
                rock.id = StringField(entry, "id", record);
                record.record = rock.id;
                if (!StartsWith(rock.id, "rock."))
                    Fail(record, "id must start with 'rock.'");
                if (!byKey.emplace(rock.id, static_cast<int>(rocks.size())).second)
                    Fail(record, "duplicate id");
                rock.numericId = IntField(entry, "numericId", record);
                if (rock.numericId < kFirstRockMaterial || rock.numericId >= kFirstLooseMaterial ||
                    !numericIds.insert(rock.numericId).second)
                    Fail(record, "numericId must be unique and in 32-511");
                rock.material = static_cast<MaterialId>(rock.numericId);
                rock.name = StringField(entry, "name", record);
                rock.nameRu = entry.value("nameRu", std::string());
                rock.rockClass = RockClassField(entry, record);
                rock.density = NumberField(entry, "density", record);
                if (!(rock.density >= 500.0 && rock.density <= 6000.0))
                    Fail(record, "density must be 500-6000 kg/m3");
                rock.digClass = IntField(entry, "digClass", record);
                if (rock.digClass < 1 || rock.digClass > 5)
                    Fail(record, "digClass must be 1-5");
                rock.color = ColorField(entry, record);
                rock.provenance = ProvenanceField(entry, record);

                if (entry.contains("modal"))
                {
                    double sum = 0.0;
                    std::set<int> used;
                    for (const Json& part : ArrayField(entry, "modal", record))
                    {
                        const std::string mineralKey = StringField(part, "mineral", record);
                        const auto found = mineralByKey.find(mineralKey);
                        if (found == mineralByKey.end())
                            Fail(record, "modal composition names unknown mineral '" + mineralKey + "'");
                        if (!used.insert(found->second).second)
                            Fail(record, "modal composition lists '" + mineralKey + "' twice");
                        const double fraction = NumberField(part, "fraction", record);
                        if (!(fraction > 0.0 && fraction <= 1.0))
                            Fail(record, "modal fraction of '" + mineralKey + "' must be in (0, 1]");
                        sum += fraction;
                        rock.modal.push_back({ found->second, fraction });
                    }
                    if (std::abs(sum - 1.0) > 1.0e-3)
                        Fail(record, "modal fractions sum to " + std::to_string(sum) + ", not 1");
                }
                rocks.push_back(std::move(rock));
            }
            (void)minerals;
        }

        void LoadOtherMaterials(const std::filesystem::path& path, std::vector<MaterialDef>& materials)
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

        void LoadColumns(const std::filesystem::path& path, const std::unordered_map<std::string, int>& rockByKey,
                         const std::vector<Rock>& rocks, std::array<ProvinceColumn, kProvinceTypeCount>& columns)
        {
            const Json root = ReadJson(path);
            Where where{ path.string(), "" };
            CheckSchemaVersion(root, where, 2);
            const Json& table = Field(root, "columns", where);
            if (!table.is_object())
                Fail(where, "field 'columns' must be an object");

            std::array<bool, kProvinceTypeCount> seen{};
            for (const auto& [name, definition] : table.items())
            {
                Where record{ path.string(), "columns." + name };
                int typeIndex = -1;
                for (int i = 0; i < kProvinceTypeCount; ++i)
                {
                    if (ProvinceTypeName(static_cast<ProvinceType>(i)) == name)
                        typeIndex = i;
                }
                if (typeIndex < 0)
                    Fail(record, "unknown province type");
                const Json& layers = Field(definition, "layers", record);
                if (!layers.is_array() || layers.empty())
                    Fail(record, "a column needs at least one layer");

                ProvinceColumn& column = columns[static_cast<std::size_t>(typeIndex)];
                column.type = static_cast<ProvinceType>(typeIndex);
                column.bottomUp.clear();

                const Json& structure = Field(definition, "structure", record);
                StructureStyle& style = column.style;
                ReadRange(structure, "dipDegrees", record, 0.0, 89.0, style.minDipDegrees, style.maxDipDegrees);
                ReadRange(structure, "layerVariability", record, 0.0, 2.0, style.minVariability, style.maxVariability);
                style.foldAmplitude = NumberField(structure, "foldAmplitude", record);
                if (!(style.foldAmplitude >= 0.0 && style.foldAmplitude <= 1.0))
                    Fail(record, "foldAmplitude must be 0-1");
                style.foldCount = IntField(structure, "foldCount", record);
                if (style.foldCount < 0 || style.foldCount > 6)
                    Fail(record, "foldCount must be 0-6");
                for (const Json& layer : layers)
                {
                    const std::string rockKey = StringField(layer, "rock", record);
                    const auto found = rockByKey.find(rockKey);
                    if (found == rockByKey.end())
                        Fail(record, "unknown rock '" + rockKey + "'");
                    ColumnLayer parsed;
                    parsed.rock = rocks[static_cast<std::size_t>(found->second)].material;
                    parsed.thicknessMeters = NumberField(layer, "thicknessM", record);
                    if (!(parsed.thicknessMeters > 0.0))
                        Fail(record, "layer thickness must be positive");
                    if (layer.contains("requires"))
                        parsed.needs = RequirementFromName(StringField(layer, "requires", record), record);
                    column.bottomUp.push_back(parsed);
                }
                if (column.bottomUp.front().needs != BiosphereRequirement::None)
                    Fail(record, "the basement layer cannot depend on the biosphere");
                seen[static_cast<std::size_t>(typeIndex)] = true;
            }
            for (int i = 0; i < kProvinceTypeCount; ++i)
            {
                if (!seen[static_cast<std::size_t>(i)])
                {
                    Fail(where, "no column for province type '" +
                                std::string(ProvinceTypeName(static_cast<ProvinceType>(i))) + "'");
                }
            }
        }
    }

    GeologyCatalog GeologyCatalog::LoadFromDirectory(const std::filesystem::path& directory)
    {
        GeologyCatalog catalog;
        LoadMinerals(directory / "minerals.json", catalog.m_minerals, catalog.m_mineralByKey);
        LoadRocks(directory / "rocks.json", catalog.m_minerals, catalog.m_mineralByKey, catalog.m_rocks,
                  catalog.m_rockByKey);

        std::vector<MaterialDef> others;
        LoadOtherMaterials(directory / "materials.json", others);

        // Rocks are materials too; their id is their numeric id.
        catalog.m_materials = std::move(others);
        for (std::size_t i = 0; i < catalog.m_rocks.size(); ++i)
        {
            const Rock& rock = catalog.m_rocks[i];
            MaterialDef material;
            material.id = rock.material;
            material.key = rock.id;
            material.name = rock.name;
            material.nameRu = rock.nameRu;
            material.blockClass = BlockClass::Rock;
            material.density = rock.density;
            material.digClass = rock.digClass;
            material.color = rock.color;
            material.rockIndex = static_cast<int>(i);
            catalog.m_materials.push_back(std::move(material));
        }
        std::sort(catalog.m_materials.begin(), catalog.m_materials.end(),
                  [](const MaterialDef& a, const MaterialDef& b) { return a.id < b.id; });

        catalog.m_materialIndex.assign(kMaterialCapacity, -1);
        std::set<std::string> keys;
        for (std::size_t i = 0; i < catalog.m_materials.size(); ++i)
        {
            const MaterialDef& material = catalog.m_materials[i];
            if (catalog.m_materialIndex[material.id] >= 0)
            {
                throw CatalogError("materials: numeric id " + std::to_string(material.id) + " is used by both '" +
                                   catalog.m_materials[static_cast<std::size_t>(catalog.m_materialIndex[material.id])].key +
                                   "' and '" + material.key + "'");
            }
            if (!keys.insert(material.key).second)
                throw CatalogError("materials: duplicate id '" + material.key + "'");
            catalog.m_materialIndex[material.id] = static_cast<int>(i);
            catalog.m_materialByKey.emplace(material.key, static_cast<int>(i));
        }
        if (catalog.GetMaterial(kAirMaterial) == nullptr || catalog.GetMaterial(kAirMaterial)->blockClass != BlockClass::Air)
            throw CatalogError("materials: material 0 must be air");
        if (catalog.GetMaterial(kWaterMaterial) == nullptr || catalog.GetMaterial(kWaterMaterial)->blockClass != BlockClass::Fluid)
            throw CatalogError("materials: material 1 must be a fluid (water)");

        LoadColumns(directory / "province_columns.json", catalog.m_rockByKey, catalog.m_rocks, catalog.m_columns);
        return catalog;
    }

    const Mineral* GeologyCatalog::FindMineral(std::string_view id) const noexcept
    {
        const auto found = m_mineralByKey.find(std::string(id));
        return found == m_mineralByKey.end() ? nullptr : &m_minerals[static_cast<std::size_t>(found->second)];
    }

    const Rock* GeologyCatalog::FindRock(std::string_view id) const noexcept
    {
        const auto found = m_rockByKey.find(std::string(id));
        return found == m_rockByKey.end() ? nullptr : &m_rocks[static_cast<std::size_t>(found->second)];
    }

    const MaterialDef* GeologyCatalog::FindMaterial(std::string_view key) const noexcept
    {
        const auto found = m_materialByKey.find(std::string(key));
        return found == m_materialByKey.end() ? nullptr : &m_materials[static_cast<std::size_t>(found->second)];
    }

    const MaterialDef* GeologyCatalog::GetMaterial(MaterialId id) const noexcept
    {
        if (id >= m_materialIndex.size() || m_materialIndex[id] < 0)
            return nullptr;
        return &m_materials[static_cast<std::size_t>(m_materialIndex[id])];
    }

    const Rock* GeologyCatalog::GetRock(MaterialId id) const noexcept
    {
        const MaterialDef* material = GetMaterial(id);
        if (material == nullptr || material->rockIndex < 0)
            return nullptr;
        return &m_rocks[static_cast<std::size_t>(material->rockIndex)];
    }

    RgbColor GeologyCatalog::ColorOf(MaterialId id) const noexcept
    {
        const MaterialDef* material = GetMaterial(id);
        return material == nullptr ? RgbColor{ 255, 0, 255 } : material->color;
    }

    Chemistry::ElementMassFractions GeologyCatalog::RockElementMassFractions(const Rock& rock) const
    {
        if (rock.modal.empty())
            return {};
        std::vector<Chemistry::MassPart> parts;
        parts.reserve(rock.modal.size());
        for (const ModalEntry& entry : rock.modal)
        {
            const Mineral& mineral = m_minerals[static_cast<std::size_t>(entry.mineralIndex)];
            parts.push_back({ mineral.composition.MassFractions(), entry.fraction * mineral.density });
        }
        return Chemistry::Mix(parts);
    }
}
