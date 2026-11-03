#include "presentation/rendering/WorldRenderer.h"

#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/WorldOverlayRenderer.h"
#include "rlgl.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace Infinitesia
{
    namespace
    {
        constexpr float kDegreesToRadians = PI / 180.0f;

        Camera3D CreateCamera(CameraConfig config)
        {
            if (!std::isfinite(config.distance) || config.distance <= 0.0f)
            {
                throw std::invalid_argument("Camera distance must be finite and greater than zero.");
            }

            if (!std::isfinite(config.elevationDegrees) ||
                config.elevationDegrees <= 0.0f || config.elevationDegrees >= 90.0f)
            {
                throw std::invalid_argument("Camera elevation must be between zero and 90 degrees.");
            }

            if (!std::isfinite(config.fieldOfViewDegrees) ||
                config.fieldOfViewDegrees <= 0.0f || config.fieldOfViewDegrees >= 180.0f)
            {
                throw std::invalid_argument("Camera field of view must be between zero and 180 degrees.");
            }

            const float elevationRadians = config.elevationDegrees * kDegreesToRadians;
            return {
                { 0.0f, config.distance * std::sin(elevationRadians), config.distance * std::cos(elevationRadians) },
                { 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f },
                config.fieldOfViewDegrees,
                CAMERA_PERSPECTIVE
            };
        }
    }

    WorldRenderer::WorldRenderer(CameraConfig config)
        : m_camera(CreateCamera(config))
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
        for (auto& [coordinate, chunkMesh] : m_chunkMeshes)
        {
            static_cast<void>(coordinate);
            if (chunkMesh.mesh.vaoId != 0 || chunkMesh.mesh.vboId != nullptr)
            {
                UnloadMesh(chunkMesh.mesh);
                chunkMesh.mesh = {};
            }
        }
        m_chunkMeshes.clear();

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
            m_worldMaterial.shader = { rlGetShaderIdDefault(), rlGetShaderLocsDefault() };
            // The atlas owns its texture; UnloadMaterial must not free it a second time.
            m_worldMaterial.maps[MATERIAL_MAP_DIFFUSE].texture.id = rlGetTextureIdDefault();
            UnloadMaterial(m_worldMaterial);
            m_worldMaterial = {};
        }
        if (m_objectMaterial.maps != nullptr)
        {
            m_objectMaterial.shader = { rlGetShaderIdDefault(), rlGetShaderLocsDefault() };
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

    void WorldRenderer::Draw(
        const Grid& grid,
        const GameWorld& world,
        std::optional<GridCell> hoveredCell,
        std::optional<WorldObjectId> hoveredObject,
        std::optional<WorldObjectId> selectedObject,
        WorldPosition pathOrigin,
        const std::vector<GridCell>& debugPath,
        GridCell chunkViewCenter,
        int chunkRenderRadius,
        int viewLevel,
        ViewMode viewMode) const
    {
        BeginMode3D(m_camera);
        BeginShaderMode(m_worldShader);
        WorldGeometryRenderer::DrawBlocks(
            grid,
            world.terrain,
            chunkViewCenter,
            chunkRenderRadius,
            viewLevel,
            viewMode,
            m_worldMaterial,
            m_blockAtlas,
            m_chunkMeshes);
        EndShaderMode();

        rlDisableDepthMask();
        BeginShaderMode(m_shadowShader);
        WorldGeometryRenderer::DrawObjectShadows(
            grid,
            world.entities,
            chunkViewCenter,
            chunkRenderRadius,
            viewLevel,
            viewMode);
        EndShaderMode();
        rlEnableDepthMask();

        BeginShaderMode(m_worldShader);
        WorldGeometryRenderer::DrawObjects(
            grid,
            world.entities,
            m_cylinderMesh,
            m_sphereMesh,
            m_objectMaterial,
            chunkViewCenter,
            chunkRenderRadius,
            viewLevel,
            viewMode);
        EndShaderMode();

        if (hoveredCell.has_value())
        {
            WorldOverlayRenderer::DrawHoveredCell(grid, *hoveredCell);
        }
        WorldOverlayRenderer::DrawGridLines(
            grid,
            chunkViewCenter,
            chunkRenderRadius,
            chunkRenderRadius <= 2,
            viewLevel);
        WorldOverlayRenderer::DrawDebugPath(grid, pathOrigin, debugPath);
        WorldOverlayRenderer::DrawObjectHighlights(grid, world.entities, hoveredObject, selectedObject);
        EndMode3D();
    }

    Camera3D& WorldRenderer::GetCamera()
    {
        return m_camera;
    }

    const Camera3D& WorldRenderer::GetCamera() const
    {
        return m_camera;
    }
}
