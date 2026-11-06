#include "presentation/rendering/terrain/FluidMeshBuilder.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <stdexcept>
#include <vector>

#include "presentation/rendering/terrain/internal/TerrainMeshGeometry.h"
#include "raymath.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace
{
constexpr unsigned char kFluidOpacity = 150;
// Meshes built in one frame; the rest keep their previous mesh for a frame or
// two, so streaming a whole sea does not stall a single frame.
constexpr int kFluidBuildsPerFrame = 6;

template <typename T> T *CopyMeshData(const std::vector<T> &data)
{
	if (data.empty())
	{
		return nullptr;
	}
	if (data.size() > std::numeric_limits<unsigned int>::max() / sizeof(T))
	{
		throw std::length_error("Fluid mesh data exceeds the raylib allocation limit.");
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

bool IsMeshLoaded(const Mesh &mesh)
{
	return mesh.vaoId != 0 || mesh.vboId != nullptr;
}

TerrainMeshGeometry::FaceLook GetFluidLook(const FluidCell &fluid, const BlockRegistry &registry,
                                           const BlockTextureAtlas *atlas)
{
	TerrainMeshGeometry::FaceLook look{};
	const MatterDef *material = registry.GetMatter(fluid.matter);
	if (atlas != nullptr && atlas->IsLoaded())
	{
		const Rectangle *tile = material != nullptr ? atlas->Find(material->key) : nullptr;
		if (tile != nullptr)
		{
			look.tint = WHITE;
			look.tint.a = kFluidOpacity;
			look.uv = *tile;
			return look;
		}
		const RgbColor color = registry.ColorOf(fluid.matter);
		look.tint = {color.r, color.g, color.b, kFluidOpacity};
		look.uv = atlas->WhiteTile();
		return look;
	}
	const RgbColor color = registry.ColorOf(fluid.matter);
	look.tint = {color.r, color.g, color.b, kFluidOpacity};
	return look;
}

FluidMeshKey GetMeshKey(const FluidChunk &chunk, int viewLevel, ViewMode mode)
{
	const int firstLevel = chunk.coordinate.y * kChunkSize;
	const bool isCut = mode == ViewMode::CutAtLevel && viewLevel < firstLevel + chunk.height;
	return {chunk.coordinate, isCut ? viewLevel : kNoFluidCut};
}

void DrawFluidMesh(const Mesh &mesh, Material material)
{
	if (mesh.vertexCount == 0)
	{
		return;
	}
	material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
	DrawMesh(mesh, material, MatrixIdentity());
}
} // namespace

Mesh BuildFluidMesh(const Grid &grid, const VoxelWorld &world, const FluidChunk &chunk, int cutLevel,
                    const BlockTextureAtlas *atlas)
{
	const int firstLevel = chunk.coordinate.y * kChunkSize;
	const bool isCut = cutLevel != kNoFluidCut;
	const int topLocalY = isCut ? std::min(cutLevel - firstLevel, chunk.height - 1) : chunk.height - 1;
	if (topLocalY < 0)
	{
		return {};
	}

	const BlockRegistry &registry = world.Registry();
	const float cellSize = grid.GetCellSize();
	const float worldLeft = -grid.GetWorldWidth() * 0.5f;
	const float worldFront = -grid.GetWorldDepth() * 0.5f;
	const float cutTopY = isCut ? static_cast<float>(cutLevel + 1) * cellSize : 0.0f;
	TerrainMeshGeometry::MeshData meshData;

	for (int localZ = 0; localZ < chunk.depth; ++localZ)
	{
		for (int localX = 0; localX < chunk.width; ++localX)
		{
			int topY = -1;
			const FluidCell *topFluid = nullptr;
			for (int localY = topLocalY; localY >= 0; --localY)
			{
				const std::size_t index = (static_cast<std::size_t>(localZ) * static_cast<std::size_t>(chunk.height) +
				                           static_cast<std::size_t>(localY)) *
				                              static_cast<std::size_t>(chunk.width) +
				                          static_cast<std::size_t>(localX);
				if (chunk.cells[index].amountMb > 0)
				{
					topY = firstLevel + localY;
					topFluid = &chunk.cells[index];
					break;
				}
			}
			if (topFluid == nullptr)
			{
				continue;
			}

			const int x = chunk.coordinate.x * kChunkSize + localX;
			const int z = chunk.coordinate.z * kChunkSize + localZ;
			// The surface of a column that continues into the chunk above is
			// drawn there, not at the chunk boundary.
			if (!isCut || topY < cutLevel)
			{
				const FluidCell *above = world.GetFluid({x, topY + 1, z});
				if (above != nullptr && above->amountMb > 0)
				{
					continue;
				}
			}

			const Block *block = world.GetBlock({x, topY, z});
			if (block != nullptr && registry.Occupied(block->state))
			{
				continue;
			}

			float surfaceY = static_cast<float>(topY) * cellSize +
			                 static_cast<float>(topFluid->amountMb) / static_cast<float>(kFullFluidCellMb) * cellSize;
			if (isCut)
			{
				surfaceY = std::min(surfaceY, cutTopY);
			}
			if (surfaceY <= static_cast<float>(topY) * cellSize)
			{
				continue;
			}

			// One block adds at most 20 vertices; indices are 16-bit.
			if (meshData.vertices.size() / 3 + 4 > std::numeric_limits<unsigned short>::max())
			{
				break;
			}

			const float left = worldLeft + static_cast<float>(x) * cellSize;
			const float front = worldFront + static_cast<float>(z) * cellSize;
			const std::array<Vector3, 4> corners{
			    Vector3{left, surfaceY, front}, Vector3{left + cellSize, surfaceY, front},
			    Vector3{left + cellSize, surfaceY, front + cellSize}, Vector3{left, surfaceY, front + cellSize}};
			meshData.AppendFace(corners, Vector3{0.0f, 1.0f, 0.0f}, GetFluidLook(*topFluid, registry, atlas));
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

void DrawFluids(const Grid &grid, const VoxelWorld &world, GridCell viewCenter, int chunkRadius, int viewLevel,
                ViewMode mode, Material material, const BlockTextureAtlas &atlas, FluidMeshCache &meshCache)
{
	const int centerChunkX = viewCenter.x / kChunkSize;
	const int centerChunkZ = viewCenter.z / kChunkSize;
	int buildsLeft = kFluidBuildsPerFrame;
	std::set<FluidMeshKey> wanted;

	for (const auto &[coordinate, chunk] : world.Fluids().Chunks())
	{
		if (std::abs(coordinate.x - centerChunkX) > chunkRadius ||
		    std::abs(coordinate.z - centerChunkZ) > chunkRadius ||
		    !IsLevelShown(mode, coordinate.y * kChunkSize, viewLevel))
		{
			continue;
		}

		const FluidMeshKey key = GetMeshKey(chunk, viewLevel, mode);
		wanted.insert(key);
		auto cached = meshCache.find(key);
		if (key.second == kNoFluidCut && !chunk.hasSurface)
		{
			// Every column of this chunk is full water under another chunk: it can
			// only produce an empty mesh, so do not spend a build on it.
			if (cached != meshCache.end())
			{
				if (IsMeshLoaded(cached->second.mesh))
				{
					UnloadMesh(cached->second.mesh);
				}
				cached->second = FluidMeshCacheEntry{};
				cached->second.fluidRevision = chunk.revision;
			}
			continue;
		}
		if (cached != meshCache.end() && cached->second.fluidRevision == chunk.revision)
		{
			DrawFluidMesh(cached->second.mesh, material);
			continue;
		}
		if (buildsLeft == 0)
		{
			// A stale mesh is drawn until its turn; a missing one is skipped.
			if (cached != meshCache.end())
			{
				DrawFluidMesh(cached->second.mesh, material);
			}
			continue;
		}
		--buildsLeft;
		if (cached != meshCache.end() && IsMeshLoaded(cached->second.mesh))
		{
			UnloadMesh(cached->second.mesh);
		}
		FluidMeshCacheEntry entry{BuildFluidMesh(grid, world, chunk, key.second, &atlas), chunk.revision};
		if (cached == meshCache.end())
		{
			cached = meshCache.emplace(key, entry).first;
		}
		else
		{
			cached->second = entry;
		}
		DrawFluidMesh(cached->second.mesh, material);
	}

	for (auto cached = meshCache.begin(); cached != meshCache.end();)
	{
		if (wanted.count(cached->first) != 0)
		{
			++cached;
			continue;
		}
		if (IsMeshLoaded(cached->second.mesh))
		{
			UnloadMesh(cached->second.mesh);
		}
		cached = meshCache.erase(cached);
	}
}
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
