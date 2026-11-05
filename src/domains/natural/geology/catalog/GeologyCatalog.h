#pragma once

#include <array>
#include <filesystem>
#include <memory>

#include "domains/natural/geology/catalog/ProvinceColumn.h"
#include "matter/MatterCatalog.h"

namespace Infinitesia
{
// What geology adds to the materials: the typical stratigraphic column of each
// province type, whose layers are rocks of the material catalog. Immutable
// after loading, so it can be shared freely between threads.
class GeologyCatalog
{
public:
	// Reads province_columns.json; layers name rocks of `materials`. Throws
	// CatalogError.
	static GeologyCatalog Load(std::shared_ptr<const MatterCatalog> materials,
	                           const std::filesystem::path &columnsFile);

	const MatterCatalog &Matter() const noexcept
	{
		return *m_matter;
	}
	std::shared_ptr<const MatterCatalog> MaterialsPointer() const noexcept
	{
		return m_matter;
	}

	const ProvinceColumn &ColumnFor(ProvinceType type) const noexcept
	{
		return m_columns[static_cast<std::size_t>(type)];
	}

private:
	std::shared_ptr<const MatterCatalog> m_matter;
	std::array<ProvinceColumn, kProvinceTypeCount> m_columns;
};
} // namespace Infinitesia
