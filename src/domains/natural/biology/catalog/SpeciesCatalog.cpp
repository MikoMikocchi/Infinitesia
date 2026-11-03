#include "domains/natural/biology/catalog/SpeciesCatalog.h"

#include "materials/CatalogJson.h"

#include <algorithm>
#include <utility>

namespace Infinitesia
{
    using namespace CatalogJson;

    namespace
    {
        template <typename Enum>
        Enum EnumField(const Json& object, const char* key, const Where& where,
                       std::initializer_list<std::pair<const char*, Enum>> names)
        {
            const std::string text = StringField(object, key, where);
            for (const auto& [name, value] : names)
            {
                if (text == name)
                    return value;
            }
            Fail(where, std::string("unknown value '") + text + "' of field '" + key + "'");
        }

        Trapezoid TrapezoidField(const Json& object, const char* key, const Where& where)
        {
            const Json& value = Field(object, key, where);
            if (!value.is_array() || value.size() != 4)
                Fail(where, std::string("field '") + key + "' must be [lowest, optimumFrom, optimumTo, highest]");
            for (const Json& number : value)
            {
                if (!number.is_number())
                    Fail(where, std::string("field '") + key + "' must hold numbers");
            }
            const Trapezoid trapezoid{ value[0].get<double>(), value[1].get<double>(),
                                       value[2].get<double>(), value[3].get<double>() };
            if (!(trapezoid.lowest < trapezoid.optimumFrom && trapezoid.optimumFrom <= trapezoid.optimumTo &&
                  trapezoid.optimumTo < trapezoid.highest))
                Fail(where, std::string("field '") + key + "' must satisfy lowest < optimumFrom <= optimumTo < highest");
            return trapezoid;
        }

        double UnitField(const Json& object, const char* key, const Where& where)
        {
            const double value = NumberField(object, key, where);
            if (!(value >= 0.0 && value <= 1.0))
                Fail(where, std::string("field '") + key + "' must be 0-1");
            return value;
        }

        IntRange IntRangeField(const Json& object, const char* key, const Where& where, int lowest, int highest)
        {
            double first = 0.0;
            double second = 0.0;
            ReadRange(object, key, where, lowest, highest, first, second);
            return { static_cast<int>(first), static_cast<int>(second) };
        }

        DoubleRange DoubleRangeField(const Json& object, const char* key, const Where& where, double lowest, double highest)
        {
            DoubleRange range;
            ReadRange(object, key, where, lowest, highest, range.min, range.max);
            return range;
        }

        Niche ReadNiche(const Json& object, const Where& where)
        {
            Niche niche;
            niche.temperatureCelsius = TrapezoidField(object, "temperatureC", where);
            niche.precipitationMm = TrapezoidField(object, "precipitationMm", where);
            niche.elevationMeters = TrapezoidField(object, "elevationM", where);
            niche.maxDrySeasonMonths = NumberField(object, "maxDrySeasonMonths", where);
            if (!(niche.maxDrySeasonMonths >= 0.0 && niche.maxDrySeasonMonths <= 12.0))
                Fail(where, "maxDrySeasonMonths must be 0-12");
            niche.minSoilFertility = UnitField(object, "minSoilFertility", where);
            niche.minSoilDepth = UnitField(object, "minSoilDepth", where);
            niche.maxSlope = NumberField(object, "maxSlope", where);
            if (!(niche.maxSlope > 0.0 && niche.maxSlope <= 4.0))
                Fail(where, "maxSlope must be above 0 and at most 4");
            return niche;
        }

        bool HasTag(const MaterialDef& material, const char* tag)
        {
            return std::find(material.tags.begin(), material.tags.end(), tag) != material.tags.end();
        }

        MaterialId PlantMaterial(const Json& object, const char* key, const Where& where,
                                 const MaterialCatalog& materials, const char* requiredTag)
        {
            const std::string text = StringField(object, key, where);
            const MaterialDef* material = materials.Find(text);
            if (material == nullptr || material->blockClass != BlockClass::Plant)
                Fail(where, std::string("'") + key + "' names '" + text + "', which is not a plant material");
            if (!HasTag(*material, requiredTag))
                Fail(where, std::string("material '") + text + "' of '" + key + "' lacks the tag '" + requiredTag + "'");
            return material->id;
        }

        Species ReadSpecies(const Json& entry, const std::string& file, const MaterialCatalog& catalog)
        {
            Where where{ file, "" };
            Species species;
            species.key = StringField(entry, "id", where);
            where.record = species.key;
            if (!StartsWith(species.key, "species."))
                Fail(where, "the id must start with 'species.'");
            species.name = StringField(entry, "name", where);
            species.form = EnumField<PlantForm>(entry, "form", where,
                { { "broadleaf", PlantForm::Broadleaf }, { "conifer", PlantForm::Conifer },
                  { "palm", PlantForm::Palm }, { "shrub", PlantForm::Shrub } });
            species.niche = ReadNiche(Field(entry, "niche", where), where);

            species.heightMeters = IntRangeField(entry, "heightM", where, 1, 80);
            species.trunkRadiusMeters = DoubleRangeField(entry, "trunkRadiusM", where, 0.5, 6.0);
            const Json& crown = Field(entry, "crown", where);
            species.crownShape = EnumField<CrownShape>(crown, "shape", where,
                { { "sphere", CrownShape::Sphere }, { "cone", CrownShape::Cone },
                  { "umbrella", CrownShape::Umbrella }, { "column", CrownShape::Column } });
            species.crownRadiusMeters = DoubleRangeField(crown, "radiusM", where, 0.5, 20.0);
            species.crownStartFraction = UnitField(crown, "startFraction", where);

            const Json& branching = Field(entry, "branching", where);
            species.branchCount = IntRangeField(branching, "count", where, 0, 64);
            species.branchAngleDegrees = NumberField(branching, "angleDegrees", where);
            if (!(species.branchAngleDegrees >= 0.0 && species.branchAngleDegrees <= 90.0))
                Fail(where, "angleDegrees must be 0-90");

            species.rootDepthMeters = IntField(entry, "rootDepthM", where);
            if (species.rootDepthMeters < 0 || species.rootDepthMeters > 4)
                Fail(where, "rootDepthM must be 0-4");
            species.shadeTolerance = UnitField(entry, "shadeTolerance", where);

            const Json& materials = Field(entry, "materials", where);
            species.materials = {
                PlantMaterial(materials, "trunk", where, catalog, "tree_wood"),
                PlantMaterial(materials, "branch", where, catalog, "tree_wood"),
                PlantMaterial(materials, "twig", where, catalog, "tree_wood"),
                PlantMaterial(materials, "leaves", where, catalog, "tree_leaves"),
                PlantMaterial(materials, "root", where, catalog, "tree_wood") };

            species.logsPerCubicMeter = NumberField(Field(entry, "yield", where), "logsPerCubicM", where);
            if (!(species.logsPerCubicMeter > 0.0))
                Fail(where, "logsPerCubicM must be positive");
            return species;
        }
    }

    SpeciesCatalog SpeciesCatalog::Load(const std::filesystem::path& path, const MaterialCatalog& materials)
    {
        const Json root = ReadJson(path);
        const Where where{ path.string(), "" };
        SpeciesCatalog catalog;
        for (const Json& entry : ArrayField(root, "species", where))
        {
            Species species = ReadSpecies(entry, path.string(), materials);
            if (catalog.Find(species.key) != nullptr)
                Fail({ path.string(), species.key }, "the id is used twice");
            catalog.m_species.push_back(std::move(species));
        }
        if (catalog.m_species.empty())
            Fail(where, "the file has no species");
        return catalog;
    }

    const Species* SpeciesCatalog::Find(std::string_view key) const noexcept
    {
        const auto found = std::find_if(m_species.begin(), m_species.end(),
                                        [&](const Species& species) { return species.key == key; });
        return found == m_species.end() ? nullptr : &*found;
    }
}
