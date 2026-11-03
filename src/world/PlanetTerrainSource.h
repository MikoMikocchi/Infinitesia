#pragma once

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
    class PlanetTerrainSource final : public TerrainSource
    {
    public:
        PlanetTerrainSource(std::shared_ptr<const SettlementTerrain> terrain,
                            int gridWidth, int gridHeight, int gridDepth, int anchorLevel);

        BlockType BlockAt(GridCell cell) const override;

        // Level of the topmost solid block of a column (clamped inside the grid).
        int SurfaceLevelAt(int x, int z) const;

        // Level of the highest water block, or a negative number when there is no sea in the grid.
        int WaterTopLevel() const;

    private:
        struct Column
        {
            int surface = 0;
            SurfaceKind kind = SurfaceKind::Grass;
        };

        const Column& ColumnAt(int x, int z) const;

        std::shared_ptr<const SettlementTerrain> m_terrain;
        int m_width;
        int m_height;
        int m_depth;
        int m_anchorLevel;
        // Columns are worked out the first time a chunk touches them.
        mutable std::vector<Column> m_columns;
        mutable std::vector<std::uint8_t> m_known;
    };
}
