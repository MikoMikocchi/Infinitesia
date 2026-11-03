#pragma once

#include "chemistry/Composition.h"
#include "geology/catalog/CatalogTypes.h"

#include <array>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Infinitesia
{
    // A catalog file is missing, malformed or inconsistent; the message names the file
    // and the record.
    class CatalogError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // The geology content registries, loaded from JSON and validated as a whole:
    // minerals (with parsed compositions), rocks (as mixtures of minerals), the other
    // materials of the world, and the typical stratigraphic column of each province
    // type. Immutable after loading, so it can be shared freely between threads.
    class GeologyCatalog
    {
    public:
        static constexpr int kMaterialCapacity = kFirstReservedMaterial;

        // Reads minerals.json, rocks.json, materials.json and province_columns.json from
        // `directory`. Throws CatalogError.
        static GeologyCatalog LoadFromDirectory(const std::filesystem::path& directory);

        std::span<const Mineral> Minerals() const noexcept { return m_minerals; }
        std::span<const Rock> Rocks() const noexcept { return m_rocks; }
        // Every material, sorted by id: air and fluids, then rocks, then loose ground.
        std::span<const MaterialDef> Materials() const noexcept { return m_materials; }

        const Mineral* FindMineral(std::string_view id) const noexcept;
        const Rock* FindRock(std::string_view id) const noexcept;
        const MaterialDef* FindMaterial(std::string_view key) const noexcept;
        // nullptr for an id that is not in the catalog (including kUnspecifiedMaterial).
        const MaterialDef* GetMaterial(MaterialId id) const noexcept;
        const Rock* GetRock(MaterialId id) const noexcept;

        const ProvinceColumn& ColumnFor(ProvinceType type) const noexcept
        {
            return m_columns[static_cast<std::size_t>(type)];
        }

        // Colour of a material; magenta for an unknown id so mistakes are visible.
        RgbColor ColorOf(MaterialId id) const noexcept;

        // Element mass fractions of a rock, from its minerals. The modal composition is by
        // volume, so each mineral is weighted by its density. Empty when the rock has no
        // modal composition.
        Chemistry::ElementMassFractions RockElementMassFractions(const Rock& rock) const;

    private:
        std::vector<Mineral> m_minerals;
        std::vector<Rock> m_rocks;
        std::vector<MaterialDef> m_materials;
        std::array<ProvinceColumn, kProvinceTypeCount> m_columns;
        std::unordered_map<std::string, int> m_mineralByKey;
        std::unordered_map<std::string, int> m_rockByKey;
        std::unordered_map<std::string, int> m_materialByKey;
        std::vector<int> m_materialIndex; // MaterialId -> index in m_materials, -1 when absent
    };
}
