#include "domains/natural/geology/catalog/GeologyCatalog.h"

#include "domains/natural/geology/catalog/ColumnLoader.h"

namespace Infinitesia
{
    GeologyCatalog GeologyCatalog::Load(std::shared_ptr<const MaterialCatalog> materials,
                                        const std::filesystem::path& columnsFile)
    {
        GeologyCatalog catalog;
        catalog.m_materials = std::move(materials);
        LoadColumns(columnsFile, *catalog.m_materials, catalog.m_columns);
        return catalog;
    }
}
