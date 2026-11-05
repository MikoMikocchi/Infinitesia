#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

#include "world/Chunk.h"
#include "world/Fluid.h"
#include "world/Grid.h"

namespace Infinitesia
{
class TerrainSource;
class VoxelWorld;

// The fluid of one block chunk, stored next to the blocks but apart from them.
// Dry chunks are not kept at all.
struct FluidChunk
{
	ChunkCoordinate coordinate;
	int width = 0;
	int height = 0;
	int depth = 0;
	// A player or a block change touched this water. Unmodified fluid chunks can
	// be dropped with their block chunk and rebuilt from the terrain.
	bool modified = false;
	std::vector<FluidCell> cells; // indexed like WorldChunk::blocks
};

// The changeable fluid layer of the world (docs/tasks/MVP_FLUID_FIELD.md): one
// fluid and one amount per cell, independent of what block stands there. It is
// filled from TerrainSource::FluidAt when a block chunk is generated, then
// settles by itself: down first, then sideways, towards lower levels, without
// pressure. A sea cell is an endless source; lake water is finite.
class FluidField
{
public:
	FluidField(int gridWidth, int gridHeight, int gridDepth);

	// nullptr outside the grid, in a dry chunk, or in a cell that never held
	// fluid.
	const FluidCell *At(GridCell cell) const;
	FluidCell *At(GridCell cell);

	// Writes a cell and wakes it and its neighbours for the next Step. Throws
	// std::out_of_range outside the grid.
	void Set(GridCell cell, FluidCell value);

	// Fills the fluid of a freshly generated block chunk from the terrain.
	void InitializeChunk(const WorldChunk &chunk, const TerrainSource &terrain);

	// Drops a chunk whose blocks are being unloaded too. Modified fluid is kept
	// by the world's unload policy and never reaches this.
	void RemoveChunk(ChunkCoordinate coordinate);

	// A block changed: wake the cells around it, and when a solid block was put
	// into fluid, push that fluid out (up first, then sideways); what finds no
	// room is lost (an accepted MVP simplification).
	void OnBlockChanged(GridCell cell, const VoxelWorld &world);

	// One simulation tick: move the active fluid down, then sideways.
	void Step(const VoxelWorld &world);

	// Changes whenever a cell changed, so meshes know they are stale.
	std::uint64_t Revision() const noexcept
	{
		return m_revision;
	}

	const std::map<ChunkCoordinate, FluidChunk> &Chunks() const noexcept
	{
		return m_chunks;
	}

	bool Contains(GridCell cell) const noexcept;

private:
	// The most that moves between two cells in one tick, as a fraction of their
	// difference. Equalising with a half keeps water from sloshing forever.
	static constexpr int kSideFlowDivisor = 2;

	FluidChunk &GetOrCreateChunk(ChunkCoordinate coordinate);
	FluidCell *AtRaw(GridCell cell);
	const FluidCell *AtRaw(GridCell cell) const;
	std::size_t IndexIn(const FluidChunk &chunk, GridCell cell) const;

	// Writes without touching the active set. Returns whether the cell changed.
	bool WriteCell(GridCell cell, FluidCell value);
	// Marks the fluid chunk of a cell, and the chunks around it, as changed by
	// the player. Natural flow in Step does not mark anything: it can be rebuilt.
	void MarkModified(GridCell cell);
	void MarkAroundModified(GridCell cell);
	void Activate(GridCell cell);
	void ActivateAround(GridCell cell);
	// Whether a cell can take this fluid: it is empty or already holds the same
	// fluid and not a full cell.
	bool CanAccept(GridCell cell, MatterId matter, const VoxelWorld &world) const;
	// Moves at most `maximumMb` from one cell to another; returns what moved.
	std::uint16_t Transfer(GridCell from, GridCell to, std::uint16_t maximumMb, const VoxelWorld &world);
	// Puts fluid into a cell, up to its free room; returns how much is left over.
	std::uint16_t Deposit(FluidCell fluid, GridCell to, const VoxelWorld &world);

	// Dwarf Fortress-style pressure: water that cannot go down traces a path
	// through full 7/7 water and teleports a level to an open cell at or below
	// the first full cell of that path. It never rises above its source.
	void PropagatePressure(GridCell cell, const VoxelWorld &world);
	void PressurizeAround(GridCell cell);

	std::map<ChunkCoordinate, FluidChunk> m_chunks;
	std::set<GridCell> m_active;
	std::set<GridCell> m_pressure;
	std::uint64_t m_revision = 0;
	int m_gridWidth;
	int m_gridHeight;
	int m_gridDepth;
};
} // namespace Infinitesia
