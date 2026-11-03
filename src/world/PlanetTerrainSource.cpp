#include "world/PlanetTerrainSource.h"

#include <algorithm>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        // Thickness of the topsoil under the surface block.
        constexpr int kTopsoilBlocks = 3;

        BlockType SurfaceBlock(SurfaceKind kind)
        {
            switch (kind)
            {
            case SurfaceKind::Grass: return BlockType::Grass;
            case SurfaceKind::Dirt:  return BlockType::Dirt;
            case SurfaceKind::Sand:  return BlockType::Sand;
            case SurfaceKind::Stone: return BlockType::Stone;
            }
            return BlockType::Stone;
        }

        BlockType TopsoilBlock(SurfaceKind kind)
        {
            switch (kind)
            {
            case SurfaceKind::Grass:
            case SurfaceKind::Dirt:  return BlockType::Dirt;
            case SurfaceKind::Sand:  return BlockType::Sand;
            case SurfaceKind::Stone: return BlockType::Stone;
            }
            return BlockType::Stone;
        }
    }

    PlanetTerrainSource::PlanetTerrainSource(
        std::shared_ptr<const SettlementTerrain> terrain,
        int gridWidth, int gridHeight, int gridDepth, int anchorLevel)
        : m_terrain(std::move(terrain))
        , m_width(gridWidth)
        , m_height(gridHeight)
        , m_depth(gridDepth)
        , m_anchorLevel(anchorLevel)
    {
        if (m_terrain == nullptr || gridWidth <= 0 || gridHeight < 4 || gridDepth <= 0 ||
            anchorLevel < 0 || anchorLevel >= gridHeight)
        {
            throw std::invalid_argument("Planet terrain source needs a terrain and a valid grid.");
        }
        const std::size_t columns = static_cast<std::size_t>(gridWidth) * static_cast<std::size_t>(gridDepth);
        m_columns.resize(columns);
        m_known.assign(columns, 0);
    }

    const PlanetTerrainSource::Column& PlanetTerrainSource::ColumnAt(int x, int z) const
    {
        const std::size_t index = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width) +
                                  static_cast<std::size_t>(x);
        if (m_known[index] == 0)
        {
            const SurfaceColumn column = m_terrain->Column(x - m_width / 2, m_depth / 2 - z);
            m_columns[index].surface = std::clamp(m_anchorLevel + column.heightBlocks, 1, m_height - 2);
            m_columns[index].kind = column.underSea ? SurfaceKind::Sand : column.kind;
            m_known[index] = 1;
        }
        return m_columns[index];
    }

    int PlanetTerrainSource::SurfaceLevelAt(int x, int z) const
    {
        x = std::clamp(x, 0, m_width - 1);
        z = std::clamp(z, 0, m_depth - 1);
        return ColumnAt(x, z).surface;
    }

    int PlanetTerrainSource::WaterTopLevel() const
    {
        // Ground at elevation k metres fills level anchor + k - origin, and the sea
        // covers everything below 0 m.
        const int top = m_anchorLevel + m_terrain->SeaLevelBlocks() - 1;
        return std::min(top, m_height - 1);
    }

    BlockType PlanetTerrainSource::BlockAt(GridCell cell) const
    {
        if (cell.x < 0 || cell.x >= m_width || cell.y < 0 || cell.y >= m_height ||
            cell.z < 0 || cell.z >= m_depth)
        {
            return BlockType::Air;
        }

        const Column& column = ColumnAt(cell.x, cell.z);
        if (cell.y > column.surface)
        {
            return cell.y <= WaterTopLevel() ? BlockType::Water : BlockType::Air;
        }
        if (cell.y == column.surface)
        {
            return SurfaceBlock(column.kind);
        }
        if (cell.y >= column.surface - kTopsoilBlocks)
        {
            return TopsoilBlock(column.kind);
        }
        return BlockType::Stone;
    }
}
