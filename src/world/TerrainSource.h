#pragma once

#include "world/Block.h"
#include "world/Grid.h"

#include <cstdint>

namespace Infinitesia
{
    // Decides what a world cell is made of before anything has been dug or built.
    // Must be deterministic: the same cell always gives the same block.
    class TerrainSource
    {
    public:
        virtual ~TerrainSource() = default;
        virtual BlockType BlockAt(GridCell cell) const = 0;
    };

    // The original flat world: a value-noise patchwork on a single surface layer.
    class NoiseTerrainSource final : public TerrainSource
    {
    public:
        NoiseTerrainSource(int gridHeight, std::uint32_t seed);

        BlockType BlockAt(GridCell cell) const override;

    private:
        int m_gridHeight;
        std::uint32_t m_seed;
    };
}
