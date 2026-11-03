#pragma once

#include "geology/model/GeologyModel.h"
#include "planet/region/SettlementTerrain.h"
#include "world/TerrainSource.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Infinitesia
{
    // Fills the world grid from the planet. Cell (x, z) is the 1 m column that lies
    // x - width / 2 metres east and depth / 2 - z metres north of the settlement
    // origin (z grows southward, so looking north the map reads the right way round);
    // the origin ground sits at `anchorLevel`, and one level is one metre of height.
    //
    // Each column is a surface block (grass, dirt, sand or stone by biome), a few
    // blocks of matching topsoil, and stone below. Ground below sea level is covered
    // by water up to the waterline.
    //
    // With a geology model the stone is real rock: MaterialAt gives the rock of every
    // stone block from the layers of the column's province. Without one every stone block
    // is unspecified, as before.
    class PlanetTerrainSource final : public TerrainSource
    {
    public:
        PlanetTerrainSource(std::shared_ptr<const SettlementTerrain> terrain,
                            int gridWidth, int gridHeight, int gridDepth, int anchorLevel,
                            std::shared_ptr<const GeologyModel> geology = nullptr);

        BlockType BlockAt(GridCell cell) const override;
        MaterialId MaterialAt(GridCell cell) const override;
        const GeologyCatalog* Catalog() const override;

        // What lies `depthBelowSurface` blocks under the surface block of a column of the
        // given kind (0 is the surface block itself): topsoil, then stone.
        static BlockType LayerAt(SurfaceKind kind, int depthBelowSurface);

        // Level of the topmost solid block of a column (clamped inside the grid).
        int SurfaceLevelAt(int x, int z) const;

        // Level of the highest water block, or a negative number when there is no sea in the grid.
        int WaterTopLevel() const;

    private:
        struct Column
        {
            int surface = 0;
            SurfaceKind kind = SurfaceKind::Grass;
            ProvinceId province = kNoProvince;
            int pileTopMeters = 0;
        };

        const Column& ColumnAt(int x, int z) const;

        std::shared_ptr<const SettlementTerrain> m_terrain;
        std::shared_ptr<const GeologyModel> m_geology;
        int m_width;
        int m_height;
        int m_depth;
        int m_anchorLevel;
        // Columns are worked out the first time a chunk touches them.
        mutable std::vector<Column> m_columns;
        mutable std::vector<std::uint8_t> m_known;
    };
}
