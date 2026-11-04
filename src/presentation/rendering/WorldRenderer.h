#pragma once

#include "raylib.h"
#include "world/Grid.h"
#include "gameplay/GameWorld.h"
#include "presentation/rendering/ViewMode.h"
#include "presentation/rendering/WorldGeometryRenderer.h"
#include "presentation/rendering/terrain/BlockTextureAtlas.h"

#include <optional>
#include <vector>

namespace Infinitesia
{
    struct CameraConfig
    {
        float distance;
        float elevationDegrees;
        float fieldOfViewDegrees;
    };

    class WorldRenderer
    {
    public:
        explicit WorldRenderer(CameraConfig config);
        WorldRenderer(const WorldRenderer&) = delete;
        WorldRenderer& operator=(const WorldRenderer&) = delete;

        void LoadShaders();
        void UnloadShaders();
        void Draw(
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
            ViewMode viewMode,
            bool showGrid) const;
        Camera3D& GetCamera();
        const Camera3D& GetCamera() const;

    private:
        // Sun, ambient, fog and camera position for the world shader.
        void SetLightingUniforms() const;

        Camera3D m_camera;
        Shader m_worldShader{};
        Shader m_shadowShader{};
        Material m_worldMaterial{};   // terrain: carries the block atlas
        Material m_objectMaterial{};  // objects: plain white texture, their meshes have 0..1 UVs
        BlockTextureAtlas m_blockAtlas;
        Mesh m_cylinderMesh{};
        Mesh m_sphereMesh{};
        mutable WorldGeometryRenderer::ChunkMeshCache m_chunkMeshes;
    };
}
