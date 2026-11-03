#include "world/TerrainSource.h"
#include "world/internal/WorldTerrainGen.h"

namespace Infinitesia
{
    NoiseTerrainSource::NoiseTerrainSource(int gridHeight, std::uint32_t seed)
        : m_gridHeight(gridHeight)
        , m_seed(seed)
    {
    }

    BlockType NoiseTerrainSource::BlockAt(GridCell cell) const
    {
        return WorldInternal::GenerateBlockType(m_gridHeight, m_seed, cell);
    }
}
