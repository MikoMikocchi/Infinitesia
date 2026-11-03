#pragma once

#include "blocks/BlockRegistry.h"
#include "geology/catalog/GeologyCatalog.h"
#include "geology/model/GeologyModel.h"
#include "planet/PlanetMap.h"
#include "planet/region/RegionGenerator.h"
#include "planet/region/SettlementTerrain.h"
#include "terrain_snapshot/Options.h"

#include <memory>

namespace SnapshotTool
{
    // The square window of the ground the picture covers, cut into columns.
    struct Window
    {
        double extent = 0.0;   // side in metres
        double step = 1.0;     // metres per column
        int columns = 0;       // columns along each side
    };

    // Derives the window from the options. Prints the problem and returns false when it is unusable.
    bool MakeWindow(const Options& options, Window& window);

    // The planet, geology and settlement terrain, built the way the game builds them.
    class SnapshotWorld
    {
    public:
        explicit SnapshotWorld(const Options& options);

        const Infinitesia::PlanetMap& Map() const noexcept { return m_map; }
        const Infinitesia::MaterialCatalog& Materials() const noexcept { return *m_materials; }
        const Infinitesia::GeologyCatalog& Catalog() const noexcept { return *m_catalog; }
        const Infinitesia::BlockRegistry& Registry() const noexcept { return *m_registry; }
        const Infinitesia::GeologyModel& Geology() const noexcept { return m_geology; }
        const Infinitesia::SettlementTerrain& Terrain() const noexcept { return m_terrain; }

    private:
        Infinitesia::PlanetMap m_map;
        Infinitesia::RegionGenerator m_generator;
        std::shared_ptr<const Infinitesia::MaterialCatalog> m_materials;
        std::shared_ptr<const Infinitesia::GeologyCatalog> m_catalog;
        std::shared_ptr<const Infinitesia::BlockRegistry> m_registry;
        Infinitesia::GeologyModel m_geology;
        Infinitesia::SettlementTerrain m_terrain;
    };
}
