#pragma once

#include "world/Block.h"
#include "world/Chunk.h"
#include "world/Grid.h"
#include "world/Inventory.h"
#include "world/Item.h"
#include "world/TerrainSource.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace Infinitesia
{
    enum class WorldObjectType
    {
        Worker,
        Tree,
        Item
    };

    struct WorldObjectId
    {
        std::uint64_t value;

        bool operator==(const WorldObjectId&) const = default;
    };

    struct WorldObjectPlacement
    {
        WorldObjectType type;
        GridCell cell;
        std::optional<Item> item = std::nullopt;
    };

    struct WorldObject
    {
        WorldObjectId id;
        WorldObjectType type;
        GridCell cell;
        WorldPosition position;
        std::optional<Item> item;
        Inventory inventory;
    };

    struct WorldObjectSize
    {
        float width;
        float height;
    };

    WorldObjectSize GetWorldObjectSize(WorldObjectType type, float cellSize);

    class World
    {
    public:
        World(
            const Grid& grid,
            const std::vector<WorldObjectPlacement>& initialObjects = {},
            std::uint32_t seed = 1);
        World(
            const Grid& grid,
            const std::vector<WorldObjectPlacement>& initialObjects,
            std::shared_ptr<const TerrainSource> terrain);

        WorldObjectId AddObject(
            WorldObjectType type,
            GridCell cell,
            const Grid& grid,
            std::optional<Item> item = std::nullopt);
        WorldObjectId TransformObject(
            WorldObjectId id,
            WorldObjectType type,
            std::optional<Item> item = std::nullopt);
        bool RemoveObject(WorldObjectId id);
        bool TryAddToInventory(WorldObjectId worker, const Item& item);
        bool SetObjectPosition(WorldObjectId id, WorldPosition position);
        bool SetObjectCell(WorldObjectId id, GridCell cell, const Grid& grid);
        void RequestChunksAround(GridCell center, int horizontalRadius, int verticalRadius = 0);
        std::size_t GeneratePendingChunks(std::size_t maximumChunks);
        const Block* GetBlock(GridCell cell) const;
        bool HasSkyAccess(GridCell cell) const;
        void SetBlockType(GridCell cell, BlockType type);
        std::uint64_t GetTerrainRevision() const;
        // Catalog naming the blocks' materials; nullptr when the terrain has no geology.
        const GeologyCatalog* GetCatalog() const { return m_terrain->Catalog(); }
        const std::map<ChunkCoordinate, WorldChunk>& GetChunks() const;
        std::size_t GetLoadedChunkCount() const;
        std::size_t GetTotalChunkCount() const;
        std::optional<WorldObjectId> FindObjectAt(GridCell cell) const;
        const WorldObject* FindObject(WorldObjectId id) const;
        const std::vector<WorldObject>& GetObjects() const;

    private:
        WorldObject* FindMutableObject(WorldObjectId id);
        void EnsureChunkGenerated(ChunkCoordinate coordinate);
        ChunkCoordinate GetChunkCoordinate(GridCell cell) const;
        std::size_t GetBlockIndex(const WorldChunk& chunk, GridCell cell) const;

        std::vector<WorldObject> m_objects;
        std::map<ChunkCoordinate, WorldChunk> m_chunks;
        std::vector<ChunkCoordinate> m_pendingChunks;
        int m_gridWidth;
        int m_gridHeight;
        int m_gridDepth;
        std::shared_ptr<const TerrainSource> m_terrain;
        std::uint64_t m_nextObjectId = 1;
        std::uint64_t m_terrainRevision = 0;
    };
}
