#pragma once

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/LocalGrid.h"
#include "domains/natural/planetary/region/RegionTerrain.h"

#include <cstdint>

namespace Infinitesia
{
    // The founded settlement: the grid its world is laid on and the region it was
    // founded in. The world is centred on that region's centre.
    struct SettlementSite
    {
        LocalGrid grid{};
        RegionId region{};
    };

    // What the surface of a block column is made of.
    enum class SurfaceKind : std::uint8_t
    {
        Grass,
        Dirt,
        Sand,
        Stone
    };

    struct SurfaceColumn
    {
        // Top solid block, in whole metres above the ground level of the origin column.
        int heightBlocks = 0;
        SurfaceKind kind = SurfaceKind::Grass;
        // True where the ground lies below sea level (the sea fills the column up to it).
        bool underSea = false;
        // Geological province of the planet cell under the column, and the elevation (whole
        // metres above sea level) of the planet's coarse surface there: the reference level
        // the province's rock layers are laid out from.
        ProvinceId province = kNoProvince;
        int pileTopMeters = 0;
    };

    // Turns the continuous planet terrain into block columns around an origin point of
    // a LocalGrid: the surface height of every 1 m column, what it is made of, and
    // where sea level lies. Block (0, 0) is the origin column and defines "ground
    // level" (height 0); east and north offsets count in blocks (metres).
    class SettlementTerrain
    {
    public:
        // Detail finer than this is not worth a block (heights are whole metres).
        static constexpr double kMinWavelengthMeters = 2.0;

        SettlementTerrain(const RegionTerrain& terrain, const LocalGrid& grid,
                          double originEastMeters, double originNorthMeters);

        const LocalGrid& GetGrid() const noexcept { return m_grid; }

        // The origin column's centre, metres east and north of the grid's anchor.
        double OriginEastMeters() const noexcept { return m_originEastMeters; }
        double OriginNorthMeters() const noexcept { return m_originNorthMeters; }

        // Ground elevation of the origin column, whole metres above sea level.
        int OriginElevationMeters() const noexcept { return m_originElevationMeters; }

        // Sea level in blocks above the origin ground level (negative when the origin is above the sea).
        int SeaLevelBlocks() const noexcept { return -m_originElevationMeters; }

        SurfaceColumn Column(int eastBlocks, int northBlocks) const noexcept;

        // What the ground is made of, given the planet cell's biome and the local elevation.
        static SurfaceKind ClassifySurface(Biome biome, double elevationMeters) noexcept;

    private:
        const RegionTerrain& m_terrain;
        LocalGrid m_grid;
        double m_originEastMeters;
        double m_originNorthMeters;
        int m_originElevationMeters = 0;
    };
}
