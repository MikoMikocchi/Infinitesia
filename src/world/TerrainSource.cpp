#include "world/TerrainSource.h"

#include "world/internal/WorldTerrainGen.h"

namespace Infinitesia
{
NoiseTerrainSource::NoiseTerrainSource(int gridHeight, std::uint32_t seed,
                                       std::shared_ptr<const BlockRegistry> registry)
    : m_gridHeight(gridHeight), m_seed(seed),
      m_registry(registry != nullptr ? std::move(registry)
	                                 : std::make_shared<const BlockRegistry>(BlockRegistry::BuiltIn())),
      m_matter(SurfaceMatter::Resolve(*m_registry))
{
}

BlockState NoiseTerrainSource::StateAt(GridCell cell) const
{
	return {WorldInternal::GenerateMatter(m_gridHeight, m_seed, cell, m_matter), kShapeFull};
}
} // namespace Infinitesia
