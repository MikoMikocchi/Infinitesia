#pragma once

#include "planet/region/RegionData.h"
#include "planet/region/RegionLayout.h"
#include "planet/region/RegionTerrain.h"

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

        const RegionLayout& GetLayout() const noexcept { return m_layout; }
        const RegionTerrain& GetTerrain() const noexcept { return m_terrain; }

        RegionData Generate(RegionId id) const;

        // Lattice around `center` covering radiusRegions regions in every direction.
        RegionPatch GeneratePatch(RegionId center, int radiusRegions) const;

    private:
        RegionLayout m_layout;
        RegionTerrain m_terrain;
    };

    // Small LRU cache so panning across a region map does not regenerate tiles.
    // Not thread-safe.
    class RegionCache
    {
    public:
        RegionCache(const RegionGenerator& generator, std::size_t capacity);

        std::shared_ptr<const RegionData> Get(RegionId id);
        std::size_t Size() const noexcept { return m_entries.size(); }

    private:
        struct Entry
        {
            std::shared_ptr<const RegionData> data;
            std::list<RegionId>::iterator order;
        };

        const RegionGenerator& m_generator;
        std::size_t m_capacity;
        std::list<RegionId> m_recent; // front = most recently used
        std::unordered_map<RegionId, Entry> m_entries;
    };
}
