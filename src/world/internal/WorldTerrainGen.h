#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "world/Block.h"
#include "world/Chunk.h"
#include "world/Grid.h"
#include "world/SurfaceMatter.h"
#include "world/VoxelWorld.h"

namespace Infinitesia {
namespace WorldInternal {
int GetChunkCount(int cellCount);
std::uint32_t HashCoordinates(int x, int z, std::uint32_t seed);
float SmoothNoise(float value);
float LatticeValue(int x, int z, std::uint32_t seed);
float TerrainNoise(int x, int z, std::uint32_t seed);
std::size_t GetBlockIndex(const WorldChunk& chunk, GridCell cell);
MatterId GenerateMatter(int gridHeight, std::uint32_t seed, GridCell cell,
                            const SurfaceMatter& materials);
}  // namespace WorldInternal
}  // namespace Infinitesia
