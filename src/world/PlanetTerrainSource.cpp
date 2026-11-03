#include "world/PlanetTerrainSource.h"

#include <algorithm>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        // Thickness of the topsoil under the surface block.
        constexpr int kTopsoilBlocks = 3;

        PlanetTerrainSource::Layer SurfaceLayer(SurfaceKind kind)
        {
            switch (kind)
            {
            case SurfaceKind::Grass: return PlanetTerrainSource::Layer::Turf;
            case SurfaceKind::Dirt:  return PlanetTerrainSource::Layer::Subsoil;
            case SurfaceKind::Sand:  return PlanetTerrainSource::Layer::Sand;
            case SurfaceKind::Stone: return PlanetTerrainSource::Layer::Rock;
            }
            return PlanetTerrainSource::Layer::Rock;
        }

        PlanetTerrainSource::Layer TopsoilLayer(SurfaceKind kind)
        {
            switch (kind)
            {
            case SurfaceKind::Grass:
            case SurfaceKind::Dirt:  return PlanetTerrainSource::Layer::Subsoil;
            case SurfaceKind::Sand:  return PlanetTerrainSource::Layer::Sand;
            case SurfaceKind::Stone: return PlanetTerrainSource::Layer::Rock;
            }
            return PlanetTerrainSource::Layer::Rock;
        }
    }

    PlanetTerrainSource::PlanetTerrainSource(
        std::shared_ptr<const SettlementTerrain> terrain,
        int gridWidth, int gridHeight, int gridDepth, int anchorLevel,
        std::shared_ptr<const BlockRegistry> registry,
        std::shared_ptr<const GeologyModel> geology)
        : m_terrain(std::move(terrain))
        , m_registry(std::move(registry))
        , m_geology(std::move(geology))
        , m_width(gridWidth)
        , m_height(gridHeight)
        , m_depth(gridDepth)
        , m_anchorLevel(anchorLevel)
    {
        if (m_terrain == nullptr || m_registry == nullptr || gridWidth <= 0 || gridHeight < 4 || gridDepth <= 0 ||
            anchorLevel < 0 || anchorLevel >= gridHeight)
        {
            throw std::invalid_argument("Planet terrain source needs a terrain, a registry and a valid grid.");
        }
        m_materials = SurfaceMaterials::Resolve(*m_registry);
        if (m_geology != nullptr)
        {
            for (int type = 0; type < kProvinceTypeCount; ++type)
            {
                for (const ColumnLayer& layer : m_geology->Catalog().ColumnFor(static_cast<ProvinceType>(type)).bottomUp)
                {
                    if (m_registry->GetMaterial(layer.rock) == nullptr)
                        throw std::invalid_argument("The registry does not know a rock of the geology.");
                }
            }
        }
        const std::size_t columns = static_cast<std::size_t>(gridWidth) * static_cast<std::size_t>(gridDepth);
        m_columns.resize(columns);
        m_known.assign(columns, 0);
        if (m_geology != nullptr)
        {
            m_profiles.resize(columns);
            m_profileKnown.assign(columns, 0);
        }
    }

    const GeologyModel::ColumnProfile& PlanetTerrainSource::ProfileAt(int x, int z) const
    {
        const std::size_t index = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_width) +
                                  static_cast<std::size_t>(x);
        if (m_profileKnown[index] == 0)
        {
            const Column& column = ColumnAt(x, z);
            // The centre of the 1 m column, in metres east and north of the grid's anchor.
            const double east = m_terrain->OriginEastMeters() + (x - m_width / 2) + 0.5;
            const double north = m_terrain->OriginNorthMeters() + (m_depth / 2 - z) + 0.5;
            m_profiles[index] = m_geology->ProfileAt(column.province, east, north, column.pileTopMeters);
            m_profileKnown[index] = 1;
        }
        return m_profiles[index];
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
            m_columns[index].province = column.province;
            m_columns[index].pileTopMeters = column.pileTopMeters;
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

    BlockState PlanetTerrainSource::StateAt(GridCell cell) const
    {
        if (cell.x < 0 || cell.x >= m_width || cell.y < 0 || cell.y >= m_height ||
            cell.z < 0 || cell.z >= m_depth)
        {
            return {};
        }

        const Column& column = ColumnAt(cell.x, cell.z);
        if (cell.y > column.surface)
        {
            return { cell.y <= WaterTopLevel() ? m_materials.seawater : kAirMaterial, kShapeFull };
        }
        switch (LayerAt(column.kind, column.surface - cell.y))
        {
        case Layer::Turf:    return { m_materials.turf, kShapeFull };
        case Layer::Subsoil: return { m_materials.subsoil, kShapeFull };
        case Layer::Sand:    return { m_materials.sand, kShapeFull };
        case Layer::Rock:    break;
        }
        if (m_geology == nullptr)
        {
            return { m_materials.bedrock, kShapeFull };
        }
        // A level is a metre of height: the ground of the origin column is at anchorLevel
        // and OriginElevationMeters above sea level.
        const int elevation = m_terrain->OriginElevationMeters() + (cell.y - m_anchorLevel);
        return { GeologyModel::RockAt(ProfileAt(cell.x, cell.z), elevation), kShapeFull };
    }

    PlanetTerrainSource::Layer PlanetTerrainSource::LayerAt(SurfaceKind kind, int depthBelowSurface)
    {
        if (depthBelowSurface <= 0)
        {
            return SurfaceLayer(kind);
        }
        return depthBelowSurface <= kTopsoilBlocks ? TopsoilLayer(kind) : Layer::Rock;
    }
}
