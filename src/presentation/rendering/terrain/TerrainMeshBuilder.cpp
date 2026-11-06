#include "presentation/rendering/terrain/TerrainMeshBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <vector>

#include "presentation/rendering/LightingStyle.h"
#include "presentation/rendering/terrain/internal/TerrainMeshGeometry.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace TerrainMeshBuilder
{
namespace
{
template <typename T> T *CopyMeshData(const std::vector<T> &data)
{
	if (data.empty())
	{
		return nullptr;
	}

	if (data.size() > std::numeric_limits<unsigned int>::max() / sizeof(T))
	{
		throw std::length_error("Chunk mesh data exceeds the raylib allocation limit.");
	}

	const unsigned int allocationSize = static_cast<unsigned int>(data.size() * sizeof(T));
	T *copy = static_cast<T *>(MemAlloc(allocationSize));
	if (copy == nullptr)
	{
		throw std::bad_alloc();
	}
	std::memcpy(copy, data.data(), data.size() * sizeof(T));
	return copy;
}

// The chunk and the one-cell ring around it: the edges of a chunk see their
// neighbours' faces and ambient occlusion, and looking a chunk up in the world
// for every one of those cells is the most expensive part of meshing.
constexpr int kLocalSize = kChunkSize + 2;
using LocalStates = std::array<BlockState, kLocalSize * kLocalSize * kLocalSize>;

std::size_t LocalIndex(int localX, int localY, int localZ)
{
	return (static_cast<std::size_t>(localZ) * kLocalSize + static_cast<std::size_t>(localY)) * kLocalSize +
	       static_cast<std::size_t>(localX);
}

void GatherNeighbourhood(const VoxelWorld &world, const WorldChunk &chunk, LocalStates &states)
{
	states.fill(BlockState{});
	const auto &chunks = world.GetChunks();
	const int baseX = chunk.coordinate.x * kChunkSize;
	const int baseY = chunk.coordinate.y * kChunkSize;
	const int baseZ = chunk.coordinate.z * kChunkSize;
	for (int dz = -1; dz <= 1; ++dz)
	{
		for (int dy = -1; dy <= 1; ++dy)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				const auto found =
				    chunks.find(ChunkCoordinate{chunk.coordinate.x + dx, chunk.coordinate.y + dy, chunk.coordinate.z + dz});
				if (found == chunks.end())
				{
					continue;
				}
				const WorldChunk &neighbor = found->second;
				for (int z = 0; z < neighbor.depth; ++z)
				{
					const int localZ = neighbor.coordinate.z * kChunkSize + z - baseZ + 1;
					if (localZ < 0 || localZ >= kLocalSize)
					{
						continue;
					}
					for (int y = 0; y < neighbor.height; ++y)
					{
						const int localY = neighbor.coordinate.y * kChunkSize + y - baseY + 1;
						if (localY < 0 || localY >= kLocalSize)
						{
							continue;
						}
						for (int x = 0; x < neighbor.width; ++x)
						{
							const int localX = neighbor.coordinate.x * kChunkSize + x - baseX + 1;
							if (localX < 0 || localX >= kLocalSize)
							{
								continue;
							}
							const std::size_t index =
							    (static_cast<std::size_t>(z) * static_cast<std::size_t>(neighbor.height) +
							     static_cast<std::size_t>(y)) *
							        static_cast<std::size_t>(neighbor.width) +
							    static_cast<std::size_t>(x);
							states[LocalIndex(localX, localY, localZ)] = neighbor.blocks[index].state;
						}
					}
				}
			}
		}
	}
}
} // namespace

void BuildFaceLookTable(const BlockRegistry &registry, const BlockTextureAtlas *atlas, FaceLookTable &table)
{
	const std::span<const MatterDef> matters = registry.Matter();
	table.top.resize(matters.size());
	table.side.resize(matters.size());
	for (const MatterDef &matter : matters)
	{
		table.top[matter.id] =
		    TerrainMeshGeometry::GetFaceLook(matter.id, registry, atlas, TerrainMeshGeometry::FaceKind::Top);
		table.side[matter.id] =
		    TerrainMeshGeometry::GetFaceLook(matter.id, registry, atlas, TerrainMeshGeometry::FaceKind::Side);
	}
}

Mesh BuildChunkMesh(const Grid &grid, const VoxelWorld &world, const WorldChunk &chunk, int cutLevel,
                    const FaceLookTable &faceLooks, bool lowDetail)
{
	const int firstLevel = chunk.coordinate.y * kChunkSize;
	const bool isCut = cutLevel != kNoCut;
	const int topLocalY = isCut ? std::min(cutLevel - firstLevel, chunk.height - 1) : chunk.height - 1;

	TerrainMeshGeometry::MeshData meshData;
	const BlockRegistry &registry = world.Registry();
	const float cellSize = grid.GetCellSize();
	const float worldLeft = -grid.GetWorldWidth() * 0.5f;
	const float worldFront = -grid.GetWorldDepth() * 0.5f;

	LocalStates states;
	GatherNeighbourhood(world, chunk, states);
	const auto stateAt = [&](GridCell cell) {
		const int localX = cell.x - chunk.coordinate.x * kChunkSize + 1;
		const int localY = cell.y - chunk.coordinate.y * kChunkSize + 1;
		const int localZ = cell.z - chunk.coordinate.z * kChunkSize + 1;
		if (localX < 0 || localX >= kLocalSize || localY < 0 || localY >= kLocalSize || localZ < 0 ||
		    localZ >= kLocalSize)
		{
			return BlockState{};
		}
		return states[LocalIndex(localX, localY, localZ)];
	};
	const auto lookFor = [&](MatterId matter, TerrainMeshGeometry::FaceKind kind) {
		const std::vector<TerrainMeshGeometry::FaceLook> &table =
		    kind == TerrainMeshGeometry::FaceKind::Top ? faceLooks.top : faceLooks.side;
		if (matter < table.size())
		{
			return table[matter];
		}
		return TerrainMeshGeometry::GetFaceLook(matter, registry, nullptr, kind);
	};

	// Whether light reaches a cell is the same for the cells of a column that are
	// above or below the last answer, so a column asks the world only a few
	// times.
	struct ColumnLight
	{
		int litFrom = std::numeric_limits<int>::max();
		int shadedUpTo = -1;
	};
	std::vector<ColumnLight> columnLight(static_cast<std::size_t>(chunk.width) * static_cast<std::size_t>(chunk.depth));
	const auto hasSkyAccess = [&](const Block &block, int localX, int localZ) {
		ColumnLight &light = columnLight[static_cast<std::size_t>(localZ) * static_cast<std::size_t>(chunk.width) +
		                                 static_cast<std::size_t>(localX)];
		if (block.cell.y >= light.litFrom)
		{
			return true;
		}
		if (block.cell.y <= light.shadedUpTo)
		{
			return false;
		}
		const bool lit = world.HasSkyAccess(block.cell);
		(lit ? light.litFrom : light.shadedUpTo) = block.cell.y;
		return lit;
	};
	// Occlusion must not depend on the cut level for chunks wholly below it
	// (their meshes are shared by all cut levels), so only the cells above the
	// cut, which exist only in the chunk the cut passes through, count as open.
	const TerrainMeshGeometry::SolidQuery isSolid = [&](GridCell cell) {
		if (isCut && cell.y > cutLevel)
		{
			return false;
		}
		return registry.Occupied(stateAt(cell));
	};
	const auto shade = [](TerrainMeshGeometry::FaceLook &look, float factor) {
		look.tint = {static_cast<unsigned char>(look.tint.r * factor), static_cast<unsigned char>(look.tint.g * factor),
		             static_cast<unsigned char>(look.tint.b * factor), look.tint.a};
	};

	// A top face per column plus a few sides; growing the arrays from this is
	// much cheaper than face by face.
	meshData.ReserveFaces(static_cast<std::size_t>(chunk.width) * static_cast<std::size_t>(chunk.depth) * 2);

	for (int localZ = 0; localZ < chunk.depth; ++localZ)
	{
		for (int localY = 0; localY <= topLocalY; ++localY)
		{
			for (int localX = 0; localX < chunk.width; ++localX)
			{
				// One block adds at most 20 vertices; indices are 16-bit.
				if (meshData.vertices.size() / 3 + 20 > std::numeric_limits<unsigned short>::max())
				{
					break;
				}

				const std::size_t blockIndex =
				    (static_cast<std::size_t>(localZ) * static_cast<std::size_t>(chunk.height) +
					 static_cast<std::size_t>(localY)) *
				        static_cast<std::size_t>(chunk.width) +
				    static_cast<std::size_t>(localX);
				const Block &block = chunk.blocks[blockIndex];
				if (!registry.Occupied(block.state))
				{
					continue;
				}

				const float left = worldLeft + static_cast<float>(block.cell.x) * cellSize;
				const float front = worldFront + static_cast<float>(block.cell.z) * cellSize;
				const float right = left + cellSize;
				const float back = front + cellSize;
				const float bottomY = static_cast<float>(block.cell.y) * cellSize;
				const float topY = bottomY + cellSize;

				const bool isCutTop = isCut && block.cell.y == cutLevel;
				const bool topExposed =
				    isCutTop || !registry.Occupied(stateAt(GridCell{block.cell.x, block.cell.y + 1, block.cell.z}));

				const bool lit = hasSkyAccess(block, localX, localZ);
				TerrainMeshGeometry::FaceLook topLook = lookFor(block.state.matter, TerrainMeshGeometry::FaceKind::Top);
				TerrainMeshGeometry::FaceLook sideLook = lookFor(block.state.matter, TerrainMeshGeometry::FaceKind::Side);
				if (!lit)
				{
					// Covered ground is dimmed, never black, so a trunk under its own
					// crown stays readable. The face of a cut is what the section
					// looks like, not a dark place, so it is dimmed less.
					shade(topLook,
					      isCutTop ? LightingStyle::kEnclosedCutFaceBrightness : LightingStyle::kEnclosedBrightness);
					shade(sideLook, LightingStyle::kEnclosedBrightness);
				}

				if (topExposed)
				{
					const std::array<Vector3, 4> topCorners{Vector3{left, topY, front}, Vector3{right, topY, front},
					                                        Vector3{right, topY, back}, Vector3{left, topY, back}};
					const Vector3 normal{0.0f, 1.0f, 0.0f};
					const std::array<float, 4> brightness =
					    lowDetail ? std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}
					              : TerrainMeshGeometry::ComputeCornerBrightness(
					                    isSolid, block.cell, normal,
					                    {(left + right) * 0.5f, (bottomY + topY) * 0.5f, (front + back) * 0.5f},
					                    topCorners);
					meshData.AppendFace(topCorners, normal, topLook, brightness);
				}
				meshData.AppendExposedSideFaces(isSolid, block.cell, left, right, front, back, bottomY, topY, sideLook,
				                                !lowDetail);
			}
		}
	}

	Mesh mesh{};
	if (meshData.vertices.empty())
	{
		return mesh;
	}
	mesh.vertexCount = static_cast<int>(meshData.vertices.size() / 3);
	mesh.triangleCount = static_cast<int>(meshData.indices.size() / 3);
	try
	{
		mesh.vertices = CopyMeshData(meshData.vertices);
		mesh.texcoords = CopyMeshData(meshData.texcoords);
		mesh.normals = CopyMeshData(meshData.normals);
		mesh.colors = CopyMeshData(meshData.colors);
		mesh.indices = CopyMeshData(meshData.indices);
	}
	catch (...)
	{
		MemFree(mesh.vertices);
		MemFree(mesh.texcoords);
		MemFree(mesh.normals);
		MemFree(mesh.colors);
		MemFree(mesh.indices);
		throw;
	}
	UploadMesh(&mesh, false);
	return mesh;
}
} // namespace TerrainMeshBuilder
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
