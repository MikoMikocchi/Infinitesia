#include "domains/natural/geology/catalog/GeologyCatalog.h"

#include "domains/natural/geology/catalog/ColumnLoader.h"

namespace Infinitesia
{
GeologyCatalog GeologyCatalog::Load(std::shared_ptr<const MatterCatalog> materials,
                                    const std::filesystem::path &columnsFile)
{
	GeologyCatalog catalog;
	catalog.m_matter = std::move(materials);
	LoadColumns(columnsFile, *catalog.m_matter, catalog.m_columns);
	return catalog;
}
} // namespace Infinitesia
