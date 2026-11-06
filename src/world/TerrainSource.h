#pragma once

#include <cstdint>
#include <memory>

#include "blocks/BlockRegistry.h"
#include "world/Block.h"
#include "world/Fluid.h"
#include "world/Grid.h"
#include "world/SurfaceMatter.h"

namespace Infinitesia
{
// Decides what a world cell is made of before anything has been dug or built.
// Must be deterministic: the same cell always gives the same state.
class TerrainSource
{
public:
	virtual ~TerrainSource() = default;
	virtual BlockState StateAt(GridCell cell) const = 0;

	// Fills `out` with the states of the column (x, z) from `y0` to `y1`
	// inclusive, ascending. The default asks StateAt for every cell; a terrain
	// that works its columns out once overrides this to do it for the column
	// instead of for each of its cells.
	virtual void FillColumn(int x, int z, int y0, int y1, BlockState *out) const
	{
		for (int y = y0; y <= y1; ++y)
		{
			*out++ = StateAt(GridCell{x, y, z});
		}
	}

	// What the cell would be without the plants that grow on the ground: where a
	// felled tree stood the ground shows again (roots in the soil give way to
	// soil, the air to air). The same as StateAt for a terrain without plants.
	virtual BlockState GroundStateAt(GridCell cell) const
	{
		return StateAt(cell);
	}

	// The registry that names the materials and shapes this terrain hands out.
	virtual const BlockRegistry &Registry() const = 0;

	// The fluid over the column, independent of the blocks: the block grid only
	// says where the ground is, this says what fluid lies on it and how deep.
	// Sources without fluids return a dry column.
	virtual FluidColumn FluidAt(int x, int z) const
	{
		static_cast<void>(x);
		static_cast<void>(z);
		return {};
	}
};

// The original flat world: a value-noise patchwork on a single surface layer.
class NoiseTerrainSource final : public TerrainSource
{
public:
	// Without a registry the minimum one (BlockRegistry::BuiltIn) is used.
	NoiseTerrainSource(int gridHeight, std::uint32_t seed, std::shared_ptr<const BlockRegistry> registry = nullptr);

	BlockState StateAt(GridCell cell) const override;
	const BlockRegistry &Registry() const override
	{
		return *m_registry;
	}

private:
	int m_gridHeight;
	std::uint32_t m_seed;
	std::shared_ptr<const BlockRegistry> m_registry;
	SurfaceMatter m_matter;
};
} // namespace Infinitesia
