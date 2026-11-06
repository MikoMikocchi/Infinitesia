#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "world/Block.h"
#include "world/Chunk.h"
#include "world/FluidField.h"
#include "world/Grid.h"
#include "world/TerrainSource.h"

namespace Infinitesia
{
// The block terrain: chunks of blocks generated on demand from a TerrainSource.
// It knows nothing about what stands on it (see gameplay/entities).
class VoxelWorld
{
public:
	VoxelWorld(const Grid &grid, std::uint32_t seed = 1);
	VoxelWorld(const Grid &grid, std::shared_ptr<const TerrainSource> terrain);
	~VoxelWorld();

	VoxelWorld(const VoxelWorld &) = delete;
	VoxelWorld &operator=(const VoxelWorld &) = delete;

	void RequestChunksAround(GridCell center, int horizontalRadius, int verticalRadius = 0);
	// Generates the pending chunks, at most `maximumChunks` and for at most
	// `budgetMs` milliseconds (0 = no time limit): a fast frame fills the view
	// quickly, a slow one does not stall. Returns how many chunks were generated.
	// With background generation on, the work happens in PumpChunkGeneration
	// instead.
	std::size_t GeneratePendingChunks(std::size_t maximumChunks, double budgetMs = 0.0);
	// Generates the blocks of pending chunks on a worker thread. Off by default:
	// tests and tools generate on the calling thread.
	void SetBackgroundGeneration(bool enabled);
	bool IsBackgroundGeneration() const
	{
		return m_background;
	}
	// Hands pending chunks to the worker and installs the finished ones. Call it
	// once per frame on the main thread; does nothing when background generation
	// is off.
	void PumpChunkGeneration();
	// Drops chunks outside the view that nobody has changed and that have no
	// fluid to keep alive. They are regenerated from the terrain source when
	// the camera returns. Modified chunks and chunks owning fluid stay.
	void UnloadChunksOutside(GridCell center, int horizontalRadius, int verticalRadius);
	// Generates the chunk holding the cell now; throws std::out_of_range outside
	// the grid.
	void EnsureChunkContaining(GridCell cell);
	const Block *GetBlock(GridCell cell) const;
	bool Contains(GridCell cell) const noexcept
	{
		return cell.x >= 0 && cell.x < m_gridWidth && cell.y >= 0 && cell.y < m_gridHeight && cell.z >= 0 &&
		       cell.z < m_gridDepth;
	}
	bool HasSkyAccess(GridCell cell) const;
	// Puts a block into the cell; throws std::invalid_argument when the registry
	// does not know its material or shape, std::out_of_range outside the grid.
	// The fluid of the cell is displaced or woken up as a result.
	void SetBlock(GridCell cell, BlockState state);
	// The fluid layer, separate from the blocks. Generated with each chunk and
	// stepped on the fixed simulation tick.
	const FluidField &Fluids() const
	{
		return m_fluids;
	}
	FluidField &Fluids()
	{
		return m_fluids;
	}
	// nullptr when the cell holds no fluid (or its chunk is not generated).
	const FluidCell *GetFluid(GridCell cell) const;
	// One tick of the fluid simulation.
	void StepFluids();
	std::uint64_t GetTerrainRevision() const;
	// What the cell would be without plants (TerrainSource::GroundStateAt); air
	// outside the grid.
	BlockState GroundStateAt(GridCell cell) const;
	// The registry that names the blocks' materials and shapes and answers every
	// rule about them.
	const BlockRegistry &Registry() const
	{
		return m_terrain->Registry();
	}
	const std::unordered_map<ChunkCoordinate, WorldChunk, ChunkCoordinateHash> &GetChunks() const;
	std::size_t GetLoadedChunkCount() const;
	std::size_t GetTotalChunkCount() const;

private:
	// The blocks of a chunk, built off the chunk table so a worker can fill them:
	// it only reads the terrain source.
	std::unique_ptr<WorldChunk> BuildChunkBlocks(ChunkCoordinate coordinate) const;
	// Puts a finished chunk into the table, its fluid and its neighbours'
	// revisions. Main thread only.
	void InstallChunk(std::unique_ptr<WorldChunk> chunk);
	void RunChunkWorker();
	void StopChunkWorker();
	void EnsureChunkGenerated(ChunkCoordinate coordinate);
	ChunkCoordinate GetChunkCoordinate(GridCell cell) const;
	std::size_t GetBlockIndex(const WorldChunk &chunk, GridCell cell) const;
	// The highest cell of a column that stops the light (-1 for none). Asked once
	// per column and remembered: HasSkyAccess is then a lookup.
	int SkyTopOf(int x, int z) const;

	std::unordered_map<ChunkCoordinate, WorldChunk, ChunkCoordinateHash> m_chunks;
	std::vector<ChunkCoordinate> m_pendingChunks;
	// The next pending chunk to generate; erasing from the front of the vector
	// moved every remaining coordinate once per chunk.
	std::size_t m_pendingNext = 0;
	// The view the pending list was built for; a view that did not move to another
	// chunk needs no new list.
	ChunkCoordinate m_pendingCenter{};
	int m_pendingHorizontalRadius = -1;
	int m_pendingVerticalRadius = -1;
	FluidField m_fluids;
	// The highest cell of a column that stops the light, by z * width + x. The
	// grid is fixed, so a dense table is smaller and faster than a hash map that
	// would have to be cleared at a wide view; kSkyUnknown means "not asked yet".
	static constexpr int kSkyUnknown = -2;
	mutable std::vector<int> m_skyTops;
	int m_gridWidth;
	int m_gridHeight;
	int m_gridDepth;
	std::shared_ptr<const TerrainSource> m_terrain;
	std::uint64_t m_terrainRevision = 0;

	// Background chunk generation: the worker fills blocks, the main thread
	// installs them. The chunks in flight are capped so a fast worker does not
	// keep a whole view of finished chunks in memory.
	static constexpr std::size_t kMaxChunksInFlight = 8;
	bool m_background = false;
	std::thread m_chunkWorker;
	std::mutex m_chunkWorkerMutex;
	std::condition_variable m_chunkWorkerCv;
	std::deque<ChunkCoordinate> m_chunkWork;
	std::deque<std::unique_ptr<WorldChunk>> m_chunkReady;
	std::size_t m_chunksInFlight = 0;
	bool m_stopChunkWorker = false;
};
} // namespace Infinitesia
