#include "geology/catalog/MineralLoader.h"

#include "geology/catalog/GeologyCatalog.h"
#include "geology/catalog/internal/CatalogJson.h"

#include <set>
namespace Infinitesia
{
    using namespace CatalogInternal;

    void LoadMinerals(const std::filesystem::path& path, std::vector<Mineral>& minerals,
                      std::unordered_map<std::string, int>& byKey)
    {
        const Json root = ReadJson(path);
        Where where{ path.string(), "" };
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
}
