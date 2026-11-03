#include "geology/catalog/RockLoader.h"

#include "geology/catalog/GeologyCatalog.h"
#include "geology/catalog/internal/CatalogJson.h"

#include <cmath>
#include <set>
namespace Infinitesia
{
    using namespace CatalogInternal;

    namespace
    {
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
    }

    void LoadRocks(const std::filesystem::path& path, const std::unordered_map<std::string, int>& mineralByKey,
                   std::vector<Rock>& rocks, std::unordered_map<std::string, int>& byKey)
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
    }
}
