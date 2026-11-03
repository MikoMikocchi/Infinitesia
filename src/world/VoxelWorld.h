#pragma once

#include "world/Block.h"
#include "world/Chunk.h"
#include "world/Grid.h"
#include "world/TerrainSource.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace Infinitesia
{
    // The block terrain: chunks of blocks generated on demand from a TerrainSource.
    // It knows nothing about what stands on it (see gameplay/entities).
    class VoxelWorld
    {
    public:
        VoxelWorld(const Grid& grid, std::uint32_t seed = 1);
        VoxelWorld(const Grid& grid, std::shared_ptr<const TerrainSource> terrain);

        void RequestChunksAround(GridCell center, int horizontalRadius, int verticalRadius = 0);
        std::size_t GeneratePendingChunks(std::size_t maximumChunks);
        // Generates the chunk holding the cell now; throws std::out_of_range outside the grid.
        void EnsureChunkContaining(GridCell cell);
        const Block* GetBlock(GridCell cell) const;
        bool HasSkyAccess(GridCell cell) const;
        void SetBlockType(GridCell cell, BlockType type);
        std::uint64_t GetTerrainRevision() const;
        // Catalog naming the blocks' materials; nullptr when the terrain has no geology.
        const GeologyCatalog* GetCatalog() const { return m_terrain->Catalog(); }
        const std::map<ChunkCoordinate, WorldChunk>& GetChunks() const;
        std::size_t GetLoadedChunkCount() const;
        std::size_t GetTotalChunkCount() const;

    private:
        void EnsureChunkGenerated(ChunkCoordinate coordinate);
        ChunkCoordinate GetChunkCoordinate(GridCell cell) const;
        std::size_t GetBlockIndex(const WorldChunk& chunk, GridCell cell) const;

        std::map<ChunkCoordinate, WorldChunk> m_chunks;
        std::vector<ChunkCoordinate> m_pendingChunks;
        int m_gridWidth;
        int m_gridHeight;
        int m_gridDepth;
        std::shared_ptr<const TerrainSource> m_terrain;
        std::uint64_t m_terrainRevision = 0;
    };
}
