#pragma once

#include "domains/natural/geology/catalog/ProvinceColumn.h"
#include "materials/MaterialCatalog.h"

#include <array>
#include <filesystem>
#include <memory>

namespace Infinitesia
{
    // What geology adds to the materials: the typical stratigraphic column of each province type,
    // whose layers are rocks of the material catalog. Immutable after loading, so it can be shared
    // freely between threads.
    class GeologyCatalog
    {
    public:
        // Reads province_columns.json; layers name rocks of `materials`. Throws CatalogError.
        static GeologyCatalog Load(std::shared_ptr<const MaterialCatalog> materials,
                                   const std::filesystem::path& columnsFile);

        const MaterialCatalog& Materials() const noexcept { return *m_materials; }
        std::shared_ptr<const MaterialCatalog> MaterialsPointer() const noexcept { return m_materials; }

        const ProvinceColumn& ColumnFor(ProvinceType type) const noexcept
        {
            return m_columns[static_cast<std::size_t>(type)];
        }

    private:
        std::shared_ptr<const MaterialCatalog> m_materials;
        std::array<ProvinceColumn, kProvinceTypeCount> m_columns;
    };
}
