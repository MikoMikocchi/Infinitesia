#include "world/World.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        int GetChunkCount(int cellCount)
        {
            return (cellCount + kChunkSize - 1) / kChunkSize;
        }

        std::uint32_t HashCoordinates(int x, int z, std::uint32_t seed)
        {
            std::uint32_t value = seed;
            value ^= static_cast<std::uint32_t>(x) * 0x9E3779B9u;
            value = (value ^ (value >> 16)) * 0x85EBCA6Bu;
            value ^= static_cast<std::uint32_t>(z) * 0xC2B2AE35u;
            value = (value ^ (value >> 13)) * 0x27D4EB2Fu;
            return value ^ (value >> 16);
        }

        float SmoothNoise(float value)
        {
            return value * value * (3.0f - 2.0f * value);
        }

        float LatticeValue(int x, int z, std::uint32_t seed)
        {
            constexpr float kInverseMaximum = 1.0f / 4294967295.0f;
            return static_cast<float>(HashCoordinates(x, z, seed)) * kInverseMaximum;
        }

        float TerrainNoise(int x, int z, std::uint32_t seed)
        {
            constexpr int kNoiseScale = 12;
            const int latticeX = x / kNoiseScale;
            const int latticeZ = z / kNoiseScale;
            const float blendX = SmoothNoise(
                static_cast<float>(x % kNoiseScale) / static_cast<float>(kNoiseScale));
            const float blendZ = SmoothNoise(
                static_cast<float>(z % kNoiseScale) / static_cast<float>(kNoiseScale));
            const float top = LatticeValue(latticeX, latticeZ, seed) * (1.0f - blendX) +
                              LatticeValue(latticeX + 1, latticeZ, seed) * blendX;
            const float bottom = LatticeValue(latticeX, latticeZ + 1, seed) * (1.0f - blendX) +
                                 LatticeValue(latticeX + 1, latticeZ + 1, seed) * blendX;
            return top * (1.0f - blendZ) + bottom * blendZ;
        }

        void ValidateObjectState(WorldObjectType type, const std::optional<Item>& item)
        {
            switch (type)
            {
            case WorldObjectType::Worker:
            case WorldObjectType::Tree:
                if (item.has_value())
                {
                    throw std::invalid_argument("Non-item world objects cannot contain an item.");
                }
                return;
            case WorldObjectType::Item:
                if (!item.has_value())
                {
                    throw std::invalid_argument("World item is invalid.");
                }
                return;
            }
            throw std::invalid_argument("World object has an unsupported type.");
        }
    }

    WorldObjectSize GetWorldObjectSize(WorldObjectType type, float cellSize)
    {
        switch (type)
        {
        case WorldObjectType::Worker:
            return { cellSize * 0.62f, cellSize * 1.2f };
        case WorldObjectType::Tree:
            return { cellSize * 0.98f, cellSize * 1.95f };
        case WorldObjectType::Item:
            return { cellSize * 0.78f, cellSize * 0.36f };
        }

        throw std::invalid_argument("World object has an unsupported type.");
    }

    World::World(
        const Grid& grid,
        const std::vector<WorldObjectPlacement>& initialObjects,
        std::uint32_t seed)
        : m_seed(seed),
          m_gridWidth(grid.GetWidth()),
          m_gridHeight(grid.GetHeight()),
          m_gridDepth(grid.GetDepth())
    {
        m_objects.reserve(initialObjects.size());
        for (const WorldObjectPlacement& placement : initialObjects)
        {
            AddObject(placement.type, placement.cell, grid, placement.item);
        }
    }

    WorldObjectId World::AddObject(
        WorldObjectType type,
        GridCell cell,
        const Grid& grid,
        std::optional<Item> item)
    {
        ValidateObjectState(type, item);
        if (!grid.Contains(cell))
        {
            throw std::out_of_range("World object cell is outside the grid.");
        }
        EnsureChunkGenerated(GetChunkCoordinate(cell));
        if (FindObjectAt(cell).has_value())
        {
            throw std::invalid_argument("Grid cell is already occupied.");
        }
        if (m_nextObjectId == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("World object ID space is exhausted.");
        }

        const WorldObjectId id{ m_nextObjectId++ };
        m_objects.push_back({ id, type, cell, grid.CellToSurface(cell), std::move(item) });
        return id;
    }

    WorldObjectId World::TransformObject(
        WorldObjectId id,
        WorldObjectType type,
        std::optional<Item> item)
    {
        ValidateObjectState(type, item);

        WorldObject* object = FindMutableObject(id);
        if (object == nullptr)
        {
            throw std::out_of_range("World object does not exist.");
        }
        if (m_nextObjectId == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::overflow_error("World object ID space is exhausted.");
        }

        object->id = { m_nextObjectId++ };
        object->type = type;
        object->item = std::move(item);
        return object->id;
    }

    bool World::RemoveObject(WorldObjectId id)
    {
        for (auto object = m_objects.begin(); object != m_objects.end(); ++object)
        {
            if (object->id == id)
            {
                m_objects.erase(object);
                return true;
            }
        }
        return false;
    }

    bool World::SetObjectPosition(WorldObjectId id, WorldPosition position)
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            throw std::invalid_argument("World object position must be finite.");
        }

        WorldObject* object = FindMutableObject(id);
        if (object == nullptr)
        {
            return false;
        }
        object->position = position;
        return true;
    }

    bool World::SetObjectCell(WorldObjectId id, GridCell cell, const Grid& grid)
    {
        if (!grid.Contains(cell))
        {
            throw std::out_of_range("World object cell is outside the grid.");
        }
        for (const WorldObject& object : m_objects)
        {
            if (object.id != id && object.cell.x == cell.x &&
                object.cell.y == cell.y && object.cell.z == cell.z)
            {
                return false;
            }
        }

        WorldObject* object = FindMutableObject(id);
        if (object == nullptr)
        {
            return false;
        }
        object->cell = cell;
        object->position = grid.CellToSurface(cell);
        return true;
    }

    void World::RequestChunksAround(GridCell center, int horizontalRadius, int verticalRadius)
    {
        if (horizontalRadius < 0 || verticalRadius < 0)
        {
            throw std::invalid_argument("Chunk generation radii cannot be negative.");
        }
        if (center.x < 0 || center.x >= m_gridWidth ||
            center.y < 0 || center.y >= m_gridHeight ||
            center.z < 0 || center.z >= m_gridDepth)
        {
            throw std::out_of_range("Chunk generation center is outside the world.");
        }

        const ChunkCoordinate centerChunk = GetChunkCoordinate(center);
        const int maxChunkX = GetChunkCount(m_gridWidth) - 1;
        const int maxChunkY = GetChunkCount(m_gridHeight) - 1;
        const int maxChunkZ = GetChunkCount(m_gridDepth) - 1;
        std::vector<ChunkCoordinate> requestedChunks;

        for (int chunkY = std::max(0, centerChunk.y - verticalRadius);
             chunkY <= std::min(maxChunkY, centerChunk.y + verticalRadius);
             ++chunkY)
        {
            for (int chunkZ = std::max(0, centerChunk.z - horizontalRadius);
                 chunkZ <= std::min(maxChunkZ, centerChunk.z + horizontalRadius);
                 ++chunkZ)
            {
                for (int chunkX = std::max(0, centerChunk.x - horizontalRadius);
                     chunkX <= std::min(maxChunkX, centerChunk.x + horizontalRadius);
                     ++chunkX)
                {
                    const ChunkCoordinate coordinate{ chunkX, chunkY, chunkZ };
                    if (m_chunks.find(coordinate) == m_chunks.end())
                    {
                        requestedChunks.push_back(coordinate);
                    }
                }
            }
        }

        std::sort(
            requestedChunks.begin(),
            requestedChunks.end(),
            [centerChunk](ChunkCoordinate left, ChunkCoordinate right)
            {
                const int leftDistance =
                    std::abs(left.x - centerChunk.x) + std::abs(left.y - centerChunk.y) +
                    std::abs(left.z - centerChunk.z);
                const int rightDistance =
                    std::abs(right.x - centerChunk.x) + std::abs(right.y - centerChunk.y) +
                    std::abs(right.z - centerChunk.z);
                if (leftDistance != rightDistance)
                {
                    return leftDistance < rightDistance;
                }
                if (left.x != right.x)
                {
                    return left.x < right.x;
                }
                if (left.y != right.y)
                {
                    return left.y < right.y;
                }
                return left.z < right.z;
            });
        m_pendingChunks = std::move(requestedChunks);
    }

    std::size_t World::GeneratePendingChunks(std::size_t maximumChunks)
    {
        std::size_t generatedChunks = 0;
        while (generatedChunks < maximumChunks && !m_pendingChunks.empty())
        {
            const ChunkCoordinate coordinate = m_pendingChunks.front();
            m_pendingChunks.erase(m_pendingChunks.begin());
            EnsureChunkGenerated(coordinate);
            ++generatedChunks;
        }
        return generatedChunks;
    }

    const Block* World::GetBlock(GridCell cell) const
    {
        if (cell.x < 0 || cell.x >= m_gridWidth ||
            cell.y < 0 || cell.y >= m_gridHeight ||
            cell.z < 0 || cell.z >= m_gridDepth)
        {
            return nullptr;
        }
        const auto chunk = m_chunks.find(GetChunkCoordinate(cell));
        if (chunk == m_chunks.end())
        {
            return nullptr;
        }
        return &chunk->second.blocks[GetBlockIndex(chunk->second, cell)];
    }

    void World::SetBlockType(GridCell cell, BlockType type)
    {
        if (type != BlockType::Grass && type != BlockType::Dirt && type != BlockType::Stone)
        {
            throw std::invalid_argument("Block has an unsupported type.");
        }
        if (cell.x < 0 || cell.x >= m_gridWidth ||
            cell.y < 0 || cell.y >= m_gridHeight ||
            cell.z < 0 || cell.z >= m_gridDepth)
        {
            throw std::out_of_range("Block cell is outside the grid.");
        }
        EnsureChunkGenerated(GetChunkCoordinate(cell));
        WorldChunk& chunk = m_chunks.at(GetChunkCoordinate(cell));
        chunk.blocks[GetBlockIndex(chunk, cell)].type = type;
        ++chunk.terrainRevision;
    }

    const std::map<ChunkCoordinate, WorldChunk>& World::GetChunks() const
    {
        return m_chunks;
    }

    std::size_t World::GetLoadedChunkCount() const
    {
        return m_chunks.size();
    }

    std::size_t World::GetTotalChunkCount() const
    {
        return static_cast<std::size_t>(GetChunkCount(m_gridWidth)) *
               static_cast<std::size_t>(GetChunkCount(m_gridHeight)) *
               static_cast<std::size_t>(GetChunkCount(m_gridDepth));
    }

    std::optional<WorldObjectId> World::FindObjectAt(GridCell cell) const
    {
        for (const WorldObject& object : m_objects)
        {
            if (object.cell.x == cell.x && object.cell.y == cell.y && object.cell.z == cell.z)
            {
                return object.id;
            }
        }

        return std::nullopt;
    }

    const WorldObject* World::FindObject(WorldObjectId id) const
    {
        for (const WorldObject& object : m_objects)
        {
            if (object.id == id)
            {
                return &object;
            }
        }

        return nullptr;
    }

    WorldObject* World::FindMutableObject(WorldObjectId id)
    {
        for (WorldObject& object : m_objects)
        {
            if (object.id == id)
            {
                return &object;
            }
        }
        return nullptr;
    }

    void World::EnsureChunkGenerated(ChunkCoordinate coordinate)
    {
        if (m_chunks.find(coordinate) != m_chunks.end())
        {
            return;
        }

        const int startX = coordinate.x * kChunkSize;
        const int startY = coordinate.y * kChunkSize;
        const int startZ = coordinate.z * kChunkSize;
        const int chunkWidth = std::min(kChunkSize, m_gridWidth - startX);
        const int chunkHeight = std::min(kChunkSize, m_gridHeight - startY);
        const int chunkDepth = std::min(kChunkSize, m_gridDepth - startZ);
        WorldChunk chunk{
            coordinate,
            chunkWidth,
            chunkHeight,
            chunkDepth,
            0,
            {}
        };
        chunk.blocks.reserve(
            static_cast<std::size_t>(chunkWidth) *
            static_cast<std::size_t>(chunkHeight) *
            static_cast<std::size_t>(chunkDepth));

        for (int localZ = 0; localZ < chunkDepth; ++localZ)
        {
            for (int localY = 0; localY < chunkHeight; ++localY)
            {
                for (int localX = 0; localX < chunkWidth; ++localX)
                {
                    const GridCell cell{
                        startX + localX,
                        startY + localY,
                        startZ + localZ
                    };
                    chunk.blocks.push_back({ cell, GenerateBlockType(cell) });
                }
            }
        }

        m_chunks.emplace(coordinate, std::move(chunk));
    }

    ChunkCoordinate World::GetChunkCoordinate(GridCell cell) const
    {
        return { cell.x / kChunkSize, cell.y / kChunkSize, cell.z / kChunkSize };
    }

    std::size_t World::GetBlockIndex(const WorldChunk& chunk, GridCell cell) const
    {
        const std::size_t localX = static_cast<std::size_t>(cell.x % kChunkSize);
        const std::size_t localY = static_cast<std::size_t>(cell.y % kChunkSize);
        const std::size_t localZ = static_cast<std::size_t>(cell.z % kChunkSize);
        return (localZ * static_cast<std::size_t>(chunk.height) + localY) *
                   static_cast<std::size_t>(chunk.width) +
               localX;
    }

    BlockType World::GenerateBlockType(GridCell cell) const
    {
        if (cell.y < m_gridHeight - 1)
        {
            return cell.y >= m_gridHeight - 4 ? BlockType::Dirt : BlockType::Stone;
        }

        const float noise = TerrainNoise(cell.x, cell.z, m_seed);
        if (noise < 0.25f)
        {
            return BlockType::Dirt;
        }
        if (noise > 0.78f)
        {
            return BlockType::Stone;
        }
        return BlockType::Grass;
    }

    const std::vector<WorldObject>& World::GetObjects() const
    {
        return m_objects;
    }
}
