#pragma once

#include "domains/natural/planetary/region/LocalGrid.h"
#include "domains/natural/planetary/region/RegionData.h"
#include "domains/natural/planetary/region/RegionTerrain.h"

#include <cstddef>
#include <list>
#include <memory>
#include <unordered_map>

namespace Infinitesia
{
    // Builds regions on demand from a finished PlanetMap. The map must outlive it.
    class RegionGenerator
    {
    public:
        explicit RegionGenerator(const PlanetMap& map);

        const RegionTerrain& GetTerrain() const noexcept { return m_terrain; }
        double PlanetRadiusMeters() const noexcept { return m_radiusMeters; }

        // The grid anchored at a point on the planet.
        LocalGrid MakeGrid(CartesianPoint3D anchorDirection) const noexcept
        {
            return LocalGrid(m_radiusMeters, anchorDirection);
        }

        RegionData Generate(const LocalGrid& grid, RegionId id) const;

        // Lattice around `center` covering radiusRegions regions in every direction.
        RegionPatch GeneratePatch(const LocalGrid& grid, RegionId center, int radiusRegions) const;

    private:
        double m_radiusMeters;
        RegionTerrain m_terrain;
    };

    // Small LRU cache so panning across a region map does not regenerate tiles.
    // Bound to one grid. Not thread-safe.
    class RegionCache
    {
    public:
        RegionCache(const RegionGenerator& generator, LocalGrid grid, std::size_t capacity);

        std::shared_ptr<const RegionData> Get(RegionId id);
        std::size_t Size() const noexcept { return m_entries.size(); }

    private:
        struct Entry
        {
            std::shared_ptr<const RegionData> data;
            std::list<RegionId>::iterator order;
        };

        const RegionGenerator& m_generator;
        LocalGrid m_grid;
        std::size_t m_capacity;
        std::list<RegionId> m_recent; // front = most recently used
        std::unordered_map<RegionId, Entry> m_entries;
    };
}
