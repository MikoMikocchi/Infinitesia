#include "geology/catalog/GeologyCatalog.h"

#include "geology/catalog/ColumnLoader.h"
#include "geology/catalog/MaterialLoader.h"
#include "geology/catalog/MineralLoader.h"
#include "geology/catalog/RockLoader.h"

#include <algorithm>
#include <set>

namespace Infinitesia
{
    GeologyCatalog GeologyCatalog::LoadFromDirectory(const std::filesystem::path& directory)
    {
        GeologyCatalog catalog;
        LoadMinerals(directory / "minerals.json", catalog.m_minerals, catalog.m_mineralByKey);
        LoadRocks(directory / "rocks.json", catalog.m_mineralByKey, catalog.m_rocks, catalog.m_rockByKey);

        std::vector<MaterialDef> others;
        LoadMaterials(directory / "materials.json", others);

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
