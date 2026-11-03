#include "planet/region/RegionGenerator.h"

#include <algorithm>
#include <stdexcept>

namespace Infinitesia
{
    RegionGenerator::RegionGenerator(const PlanetMap& map)
        : m_layout(map.GetPlanet().GetRadius())
        , m_terrain(map)
    {
    }

    RegionData RegionGenerator::Generate(RegionId id) const
    {
        if (!m_layout.IsValid(id))
        {
            throw std::out_of_range("Region id is outside the planet.");
        }

        RegionData data;
        data.id = id;
        data.center = m_layout.Center(id);
        data.cellSizeMeters = m_layout.CellSizeMeters();

        const std::size_t count = static_cast<std::size_t>(RegionData::kPoints) * RegionData::kPoints;
        data.elevationMeters.resize(count);
        data.biome.resize(count);

        // The lattice resolves features down to two cells (Nyquist).
        const double minWavelength = data.cellSizeMeters * 2.0;
        const PlanetMap& map = m_terrain.GetPlanetMap();

        float minimum = 0.0f;
        float maximum = 0.0f;
        for (int y = 0; y < RegionData::kPoints; ++y)
        {
            for (int x = 0; x < RegionData::kPoints; ++x)
            {
                const CartesianPoint3D direction = m_layout.PointDirection(id, x, y);
                const PlanetFieldSample planet = m_terrain.SamplePlanet(direction);
                const float elevation = static_cast<float>(
                    m_terrain.ElevationMeters(direction, planet, minWavelength));

                const std::size_t index = RegionData::Index(x, y);
                data.elevationMeters[index] = elevation;
                data.biome[index] = map.GetRegion(planet.nearestLatitudeIndex, planet.nearestLongitudeIndex).biome;

                if (index == 0)
                {
                    minimum = maximum = elevation;
                }
                minimum = std::min(minimum, elevation);
                maximum = std::max(maximum, elevation);
            }
        }
        data.minElevationMeters = minimum;
        data.maxElevationMeters = maximum;
        return data;
    }

    RegionPatch RegionGenerator::GeneratePatch(RegionId center, int radiusRegions) const
    {
        if (!m_layout.IsValid(center))
        {
            throw std::out_of_range("Region id is outside the planet.");
        }
        if (radiusRegions < 0 || radiusRegions > 8)
        {
            throw std::invalid_argument("Patch radius must be between 0 and 8 regions.");
        }

        RegionPatch patch;
        patch.center = center;
        patch.radiusRegions = radiusRegions;
        patch.points = (2 * radiusRegions + 1) * RegionLayout::kCellsPerRegionEdge + 1;
        patch.cellSizeMeters = m_layout.CellSizeMeters();

        const std::size_t count = static_cast<std::size_t>(patch.points) * patch.points;
        patch.elevationMeters.resize(count);
        patch.biome.resize(count);
        patch.planetCell.resize(count);

        const double minWavelength = patch.cellSizeMeters * 2.0;
        const PlanetMap& map = m_terrain.GetPlanetMap();
        const int longitudeSegments = map.GetLongitudeSegmentCount();
        const int origin = radiusRegions * RegionLayout::kCellsPerRegionEdge;

        for (int y = 0; y < patch.points; ++y)
        {
            for (int x = 0; x < patch.points; ++x)
            {
                const CartesianPoint3D direction = m_layout.PointDirection(center, x - origin, y - origin);
                const PlanetFieldSample planet = m_terrain.SamplePlanet(direction);
                const std::size_t index = patch.Index(x, y);
                patch.elevationMeters[index] = static_cast<float>(
                    m_terrain.ElevationMeters(direction, planet, minWavelength));
                patch.biome[index] =
                    map.GetRegion(planet.nearestLatitudeIndex, planet.nearestLongitudeIndex).biome;
                patch.planetCell[index] = static_cast<std::uint32_t>(
                    planet.nearestLatitudeIndex * longitudeSegments + planet.nearestLongitudeIndex);
            }
        }
        return patch;
    }

    RegionCache::RegionCache(const RegionGenerator& generator, std::size_t capacity)
        : m_generator(generator)
        , m_capacity(std::max<std::size_t>(1, capacity))
    {
    }

    std::shared_ptr<const RegionData> RegionCache::Get(RegionId id)
    {
        const auto found = m_entries.find(id);
        if (found != m_entries.end())
        {
            m_recent.splice(m_recent.begin(), m_recent, found->second.order);
            return found->second.data;
        }

        auto data = std::make_shared<const RegionData>(m_generator.Generate(id));
        m_recent.push_front(id);
        m_entries.emplace(id, Entry{ data, m_recent.begin() });

        while (m_entries.size() > m_capacity)
        {
            m_entries.erase(m_recent.back());
            m_recent.pop_back();
        }
        return data;
    }
}
