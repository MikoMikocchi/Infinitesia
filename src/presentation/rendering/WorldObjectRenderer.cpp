#include <algorithm>
#include <cmath>

#include "presentation/rendering/DepthShading.h"
#include "presentation/rendering/WorldGeometryRenderer.h"
#include "raymath.h"
#include "rlgl.h"

namespace Infinitesia
{
namespace WorldGeometryRenderer
{
namespace
{
constexpr float kDegreesToRadians = PI / 180.0f;
constexpr float kShadowHeight = 0.006f;

bool IsChunkVisible(GridCell cell, GridCell viewCenter, int chunkRadius, int viewLevel, ViewMode mode)
{
	const int chunkX = cell.x / kChunkSize;
	const int chunkZ = cell.z / kChunkSize;
	const int centerChunkX = viewCenter.x / kChunkSize;
	const int centerChunkZ = viewCenter.z / kChunkSize;
	return IsLevelShown(mode, cell.y, viewLevel) && std::abs(chunkX - centerChunkX) <= chunkRadius &&
	       std::abs(chunkZ - centerChunkZ) <= chunkRadius;
}

void DrawLitMesh(Mesh mesh, Material material, WorldPosition position, Vector3 scale, Color color,
                 float rotationAroundZ = 0.0f)
{
	const Color previousDiffuseColor = material.maps[MATERIAL_MAP_DIFFUSE].color;
	material.maps[MATERIAL_MAP_DIFFUSE].color = color;
	Matrix transform = MatrixScale(scale.x, scale.y, scale.z);
	if (rotationAroundZ != 0.0f)
	{
		transform = MatrixMultiply(transform, MatrixRotate({0.0f, 0.0f, 1.0f}, rotationAroundZ * kDegreesToRadians));
	}
	transform = MatrixMultiply(transform, MatrixTranslate(position.x, position.y, position.z));
	DrawMesh(mesh, material, transform);
	material.maps[MATERIAL_MAP_DIFFUSE].color = previousDiffuseColor;
}

void DrawItem(const Grid &grid, const WorldObject &object, Mesh cylinderMesh, Material material, int levelsBelow)
{
	if (!object.item.has_value())
	{
		return;
	}

	const WorldPosition center = object.position;
	const float cellSize = grid.GetCellSize();
	if (object.item->GetDef().visual != ItemVisual::Logs)
	{
		DrawCube({center.x, center.y + cellSize * 0.16f, center.z}, cellSize * 0.3f, cellSize * 0.3f, cellSize * 0.3f,
		         DepthShading::Apply({190, 170, 125, 255}, levelsBelow));
		return;
	}

	const int quantity = object.item->GetQuantity();
	const int bottomRowCount = static_cast<int>((std::sqrt(8.0 * static_cast<double>(quantity) + 1.0) - 1.0) * 0.5);
	const float logRadius = cellSize * 0.09f;
	const float logLength = cellSize * 0.62f;
	const float rowSpacing = logRadius * 1.8f;
	int remainingLogs = quantity;
	int row = 0;

	while (remainingLogs > 0)
	{
		const int desiredRowCount = row < bottomRowCount ? bottomRowCount - row : remainingLogs;
		const int rowCount = std::min(desiredRowCount, remainingLogs);
		for (int log = 0; log < rowCount; ++log)
		{
			const float offset = (static_cast<float>(log) - static_cast<float>(rowCount - 1) * 0.5f) * rowSpacing;
			DrawLitMesh(cylinderMesh, material,
			            {center.x + logLength * 0.5f, center.y + logRadius + static_cast<float>(row) * rowSpacing,
			             center.z + offset},
			            {logRadius, logLength, logRadius}, DepthShading::Apply({145, 95, 55, 255}, levelsBelow), 90.0f);
		}
		remainingLogs -= rowCount;
		++row;
	}
}

void DrawWorldObject(const Grid &grid, const WorldObject &object, Mesh cylinderMesh, Mesh sphereMesh, Material material,
                     int levelsBelow)
{
	const WorldPosition cellCenter = object.position;
	const float cellSize = grid.GetCellSize();

	if (object.kind->visual == ObjectVisual::Worker)
	{
		DrawLitMesh(cylinderMesh, material, cellCenter, {cellSize * 0.24f, cellSize * 0.76f, cellSize * 0.24f},
		            DepthShading::Apply({75, 135, 220, 255}, levelsBelow));
		DrawLitMesh(sphereMesh, material, {cellCenter.x, cellCenter.y + cellSize * 0.93f, cellCenter.z},
		            {cellSize * 0.20f, cellSize * 0.20f, cellSize * 0.20f},
		            DepthShading::Apply({225, 190, 150, 255}, levelsBelow));
		return;
	}

	DrawItem(grid, object, cylinderMesh, material, levelsBelow);
}

void DrawObjectShadow(const Grid &grid, const WorldObject &object)
{
	const float cellSize = grid.GetCellSize();
	const WorldObjectSize size = GetWorldObjectSize(*object.kind, cellSize);
	const float shadowScale = 1.0f + size.height * 0.18f;
	const float shadowOffset = size.height * 0.12f;
	const float halfWidth = cellSize * shadowScale * 0.5f;
	const float halfDepth = cellSize * shadowScale * 0.36f;
	const float centerX = object.position.x + shadowOffset * 0.6f;
	const float centerY = object.position.y + kShadowHeight;
	const float centerZ = object.position.z - shadowOffset * 0.45f;
	const unsigned char opacity = static_cast<unsigned char>(object.kind->shadowOpacity);

	rlBegin(RL_QUADS);
	rlColor4ub(255, 255, 255, opacity);
	rlNormal3f(0.0f, 1.0f, 0.0f);
	rlTexCoord2f(0.0f, 0.0f);
	rlVertex3f(centerX - halfWidth, centerY, centerZ - halfDepth);
	rlTexCoord2f(0.0f, 1.0f);
	rlVertex3f(centerX - halfWidth, centerY, centerZ + halfDepth);
	rlTexCoord2f(1.0f, 1.0f);
	rlVertex3f(centerX + halfWidth, centerY, centerZ + halfDepth);
	rlTexCoord2f(1.0f, 0.0f);
	rlVertex3f(centerX + halfWidth, centerY, centerZ - halfDepth);
	rlEnd();
}
} // namespace

void DrawObjectShadows(const Grid &grid, const EntityStore &world, GridCell viewCenter, int chunkRadius, int viewLevel,
                       ViewMode mode)
{
	for (const WorldObject &object : world.GetObjects())
	{
		if (IsChunkVisible(object.cell, viewCenter, chunkRadius, viewLevel, mode))
		{
			DrawObjectShadow(grid, object);
		}
	}
}

void DrawObjects(const Grid &grid, const EntityStore &world, Mesh cylinderMesh, Mesh sphereMesh, Material material,
                 GridCell viewCenter, int chunkRadius, int viewLevel, ViewMode mode)
{
	for (const WorldObject &object : world.GetObjects())
	{
		if (IsChunkVisible(object.cell, viewCenter, chunkRadius, viewLevel, mode))
		{
			const int levelsBelow = mode == ViewMode::CutAtLevel ? std::max(viewLevel - object.cell.y, 0) : 0;
			DrawWorldObject(grid, object, cylinderMesh, sphereMesh, material, levelsBelow);
		}
	}
}
} // namespace WorldGeometryRenderer
} // namespace Infinitesia
