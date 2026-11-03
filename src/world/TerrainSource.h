#pragma once

#include "blocks/BlockRegistry.h"
#include "world/Block.h"
#include "world/Grid.h"
#include "world/SurfaceMaterials.h"

#include <cstdint>
#include <memory>

namespace Infinitesia
{
    // Decides what a world cell is made of before anything has been dug or built.
    // Must be deterministic: the same cell always gives the same state.
    class TerrainSource
    {
    public:
        virtual ~TerrainSource() = default;
        virtual BlockState StateAt(GridCell cell) const = 0;

        // The registry that names the materials and shapes this terrain hands out.
        virtual const BlockRegistry& Registry() const = 0;
    };

    // The original flat world: a value-noise patchwork on a single surface layer.
    class NoiseTerrainSource final : public TerrainSource
    {
    public:
        // Without a registry the minimum one (BlockRegistry::BuiltIn) is used.
        NoiseTerrainSource(int gridHeight, std::uint32_t seed,
                           std::shared_ptr<const BlockRegistry> registry = nullptr);

        BlockState StateAt(GridCell cell) const override;
        const BlockRegistry& Registry() const override { return *m_registry; }

    private:
        int m_gridHeight;
        std::uint32_t m_seed;
        std::shared_ptr<const BlockRegistry> m_registry;
        SurfaceMaterials m_materials;
    };
}
