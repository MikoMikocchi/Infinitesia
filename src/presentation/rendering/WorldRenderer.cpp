#include "presentation/rendering/WorldRenderer.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "presentation/rendering/DepthShading.h"
#include "presentation/rendering/LightingStyle.h"
#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/WorldOverlayRenderer.h"
#include "raymath.h"
#include "rlgl.h"

namespace Infinitesia
{
namespace
{
constexpr float kDegreesToRadians = PI / 180.0f;
constexpr double kNearClipPlane = 0.2;
constexpr double kFarClipPlane = 1000.0;

Camera3D CreateCamera(CameraConfig config)
{
	if (!std::isfinite(config.distance) || config.distance <= 0.0f)
	{
		throw std::invalid_argument("Camera distance must be finite and greater than zero.");
	}

	if (!std::isfinite(config.elevationDegrees) || config.elevationDegrees <= 0.0f || config.elevationDegrees >= 90.0f)
	{
		throw std::invalid_argument("Camera elevation must be between zero and 90 degrees.");
	}

	if (!std::isfinite(config.fieldOfViewDegrees) || config.fieldOfViewDegrees <= 0.0f ||
	    config.fieldOfViewDegrees >= 180.0f)
	{
		throw std::invalid_argument("Camera field of view must be between zero and 180 degrees.");
	}

	const float elevationRadians = config.elevationDegrees * kDegreesToRadians;
	return {{0.0f, config.distance * std::sin(elevationRadians), config.distance * std::cos(elevationRadians)},
	        {0.0f, 0.0f, 0.0f},
	        {0.0f, 1.0f, 0.0f},
	        config.fieldOfViewDegrees,
	        CAMERA_PERSPECTIVE};
}
} // namespace

WorldRenderer::WorldRenderer(CameraConfig config) : m_camera(CreateCamera(config))
{
}

void WorldRenderer::LoadShaders()
{
	if (m_worldShader.id != 0 || m_shadowShader.id != 0)
	{
		throw std::logic_error("World renderer shaders are already loaded.");
	}

	const std::filesystem::path shaderDirectory =
	    std::filesystem::path(GetApplicationDirectory()) / "assets" / "shaders";
	const std::string worldShaderPath = (shaderDirectory / "world_lighting.fs").string();
	const std::string worldVertexShaderPath = (shaderDirectory / "world_lighting.vs").string();
	m_worldShader = LoadShader(worldVertexShaderPath.c_str(), worldShaderPath.c_str());
	if (m_worldShader.id == 0)
	{
		throw std::runtime_error("Failed to load the world lighting shader.");
	}

	const std::string shadowShaderPath = (shaderDirectory / "blob_shadow.fs").string();
	m_shadowShader = LoadShader(nullptr, shadowShaderPath.c_str());
	if (m_shadowShader.id == 0)
	{
		UnloadShader(m_worldShader);
		m_worldShader = {};
		throw std::runtime_error("Failed to load the blob shadow shader.");
	}

	try
	{
		m_blockAtlas.Load(std::filesystem::path(GetApplicationDirectory()) / "assets" / "textures" / "blocks");
	}
	catch (...)
	{
		UnloadShader(m_shadowShader);
		UnloadShader(m_worldShader);
		m_shadowShader = {};
		m_worldShader = {};
		throw;
	}

	m_worldMaterial = LoadMaterialDefault();
	if (m_blockAtlas.IsLoaded())
	{
		m_worldMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = m_blockAtlas.GetTexture();
	}
	m_worldMaterial.shader = m_worldShader;
	m_worldMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
	m_objectMaterial = LoadMaterialDefault();
	m_objectMaterial.shader = m_worldShader;
	m_objectMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
	m_cylinderMesh = GenMeshCylinder(1.0f, 1.0f, 8);
	m_sphereMesh = GenMeshSphere(1.0f, 8, 8);
}

void WorldRenderer::UnloadShaders()
{
	for (auto &[coordinate, chunkMesh] : m_chunkMeshes)
	{
		static_cast<void>(coordinate);
		if (chunkMesh.mesh.vaoId != 0 || chunkMesh.mesh.vboId != nullptr)
		{
			UnloadMesh(chunkMesh.mesh);
			chunkMesh.mesh = {};
		}
	}
	m_chunkMeshes.clear();

	for (auto &[coordinate, fluidMesh] : m_fluidMeshes)
	{
		static_cast<void>(coordinate);
		if (fluidMesh.mesh.vaoId != 0 || fluidMesh.mesh.vboId != nullptr)
		{
			UnloadMesh(fluidMesh.mesh);
			fluidMesh.mesh = {};
		}
	}
	m_fluidMeshes.clear();

	if (m_cylinderMesh.vaoId != 0 || m_cylinderMesh.vboId != nullptr)
	{
		UnloadMesh(m_cylinderMesh);
		m_cylinderMesh = {};
	}
	if (m_sphereMesh.vaoId != 0 || m_sphereMesh.vboId != nullptr)
	{
		UnloadMesh(m_sphereMesh);
		m_sphereMesh = {};
	}
	if (m_worldMaterial.maps != nullptr)
	{
		m_worldMaterial.shader = {rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
		// The atlas owns its texture; UnloadMaterial must not free it a second
		// time.
		m_worldMaterial.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
		UnloadMaterial(m_worldMaterial);
		m_worldMaterial = {};
	}
	if (m_objectMaterial.maps != nullptr)
	{
		m_objectMaterial.shader = {rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
		UnloadMaterial(m_objectMaterial);
		m_objectMaterial = {};
	}
	m_blockAtlas.Unload();
	if (m_shadowShader.id != 0)
	{
		UnloadShader(m_shadowShader);
		m_shadowShader = {};
	}
	if (m_worldShader.id != 0)
	{
		UnloadShader(m_worldShader);
		m_worldShader = {};
	}
}

void WorldRenderer::SetLightingUniforms() const
{
	const auto setVec3 = [&](const char *name, const float (&value)[3]) {
		SetShaderValue(m_worldShader, GetShaderLocation(m_worldShader, name), value, SHADER_UNIFORM_VEC3);
	};
	setVec3("lightDirection", LightingStyle::kSunDirection);
	setVec3("sunColor", LightingStyle::kSunColor);
	setVec3("ambientSky", LightingStyle::kAmbientSky);
	setVec3("ambientGround", LightingStyle::kAmbientGround);
	setVec3("fogColor", LightingStyle::kFogColor);

	// The fog is measured in multiples of the distance to what the camera looks
	// at, so it does not wash the view out when zoomed far away.
	const float focusDistance = Vector3Distance(m_camera.position, m_camera.target);
	const float fogParams[4]{focusDistance * LightingStyle::kFogStartRatio, focusDistance * LightingStyle::kFogEndRatio,
	                         LightingStyle::kFogStrength, LightingStyle::kFogDesaturation};
	SetShaderValue(m_worldShader, GetShaderLocation(m_worldShader, "fogParams"), fogParams, SHADER_UNIFORM_VEC4);
	const float cameraPosition[3]{m_camera.position.x, m_camera.position.y, m_camera.position.z};
	setVec3("cameraPosition", cameraPosition);
}

void WorldRenderer::Draw(const Grid &grid, const GameWorld &world, std::optional<GridCell> hoveredCell,
                         std::optional<WorldObjectId> hoveredObject, std::optional<WorldObjectId> selectedObject,
                         WorldPosition pathOrigin, const std::vector<GridCell> &debugPath, GridCell chunkViewCenter,
                         int chunkRenderRadius, int viewLevel, ViewMode viewMode, bool showGrid) const
{
	// Terrain fades with depth below the viewed level; objects are tinted on the
	// CPU instead, as their meshes are not in world space, so the shading is off
	// while they draw.
	const bool shadeDepth = viewMode == ViewMode::CutAtLevel;
	const float cellSize = grid.GetCellSize();
	const float depthShadeA[4]{static_cast<float>(viewLevel + 1) * cellSize, 1.0f / cellSize,
	                           shadeDepth ? DepthShading::kDarkenPerLevel : 0.0f, DepthShading::kMinBrightness};
	const float depthShadeB[4]{DepthShading::kDesaturatePerLevel, DepthShading::kMaxDesaturation, 0.0f, 0.0f};
	const float noDepthShadeA[4]{0.0f, 1.0f, 0.0f, 1.0f};
	const int depthShadeLocA = GetShaderLocation(m_worldShader, "depthShadeA");
	const int depthShadeLocB = GetShaderLocation(m_worldShader, "depthShadeB");
	SetShaderValue(m_worldShader, depthShadeLocA, depthShadeA, SHADER_UNIFORM_VEC4);
	SetShaderValue(m_worldShader, depthShadeLocB, depthShadeB, SHADER_UNIFORM_VEC4);

	SetLightingUniforms();

	// raylib's default near plane (0.01) leaves a 24-bit depth buffer too coarse
	// at the far zoom (about 0.1 units at 140): coplanar shadows and grid lines
	// flicker. The camera never gets closer than its minimum distance to
	// anything, so the near plane can sit much further out.
	rlSetClipPlanes(kNearClipPlane, kFarClipPlane);
	BeginMode3D(m_camera);
	BeginShaderMode(m_worldShader);
	WorldGeometryRenderer::DrawBlocks(grid, world.terrain, chunkViewCenter, chunkRenderRadius, viewLevel, viewMode,
	                                  m_worldMaterial, m_blockAtlas, m_chunkMeshes);
	EndShaderMode();

	// Water is a separate, translucent layer over the opaque ground: it never
	// writes depth, so the ground and objects behind it stay visible.
	if (!world.terrain.Fluids().Chunks().empty())
	{
		rlDisableDepthMask();
		BeginBlendMode(BLEND_ALPHA);
		BeginShaderMode(m_worldShader);
		WorldGeometryRenderer::DrawFluids(grid, world.terrain, chunkViewCenter, chunkRenderRadius, viewLevel, viewMode,
		                                  m_worldMaterial, m_blockAtlas, m_fluidMeshes);
		EndShaderMode();
		EndBlendMode();
		rlEnableDepthMask();
	}

	rlDisableDepthMask();
	BeginShaderMode(m_shadowShader);
	WorldGeometryRenderer::DrawObjectShadows(grid, world.entities, chunkViewCenter, chunkRenderRadius, viewLevel,
	                                         viewMode);
	EndShaderMode();
	rlEnableDepthMask();

	SetShaderValue(m_worldShader, depthShadeLocA, noDepthShadeA, SHADER_UNIFORM_VEC4);
	BeginShaderMode(m_worldShader);
	WorldGeometryRenderer::DrawObjects(grid, world.entities, m_cylinderMesh, m_sphereMesh, m_objectMaterial,
	                                   chunkViewCenter, chunkRenderRadius, viewLevel, viewMode);
	EndShaderMode();

	if (hoveredCell.has_value())
	{
		WorldOverlayRenderer::DrawHoveredCell(grid, *hoveredCell);
	}
	if (showGrid)
	{
		WorldOverlayRenderer::DrawGridLines(grid, chunkViewCenter, chunkRenderRadius, chunkRenderRadius <= 2,
		                                    viewLevel);
	}
	WorldOverlayRenderer::DrawDebugPath(grid, pathOrigin, debugPath, viewLevel, viewMode);
	WorldOverlayRenderer::DrawObjectHighlights(grid, world.entities, hoveredObject, selectedObject);
	EndMode3D();
	rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
}

Camera3D &WorldRenderer::GetCamera()
{
	return m_camera;
}

const Camera3D &WorldRenderer::GetCamera() const
{
	return m_camera;
}
} // namespace Infinitesia
