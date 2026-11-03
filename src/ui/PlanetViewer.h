#pragma once

#include "core/LoadProgress.h"
#include "planet/StarSystem.h"
#include "planet/PlanetMap.h"
#include "raylib.h"
#include "ui/UniqueTexture.h"

#include <vector>

namespace Infinitesia
{
    class PlanetViewer
    {
    public:
        explicit PlanetViewer(StarSystem system, LoadProgress* progress = nullptr);

        void Update();
        void Draw() const;
        void ReleaseRenderResources() noexcept;

    private:
        void HandleInput();
        void EnsureSurfaceTexture();
        Camera3D MakeCamera() const;
        void SyncGlobeMesh() const;
        void WriteCellColors(Color* out, int row, int column, bool selected) const;

        StarSystem m_system;
        PlanetMap m_planetMap;
        std::vector<Color> m_regionColors;
        std::vector<float> m_regionBrightness;
        std::vector<Vector3> m_sphereVertices;
        std::vector<Color> m_sphereVertexColors;
        std::vector<float> m_sphereVertexBrightness;
        UniqueTexture m_surfaceTexture;
        // Static globe mesh (6 vertices per cell, baked vertex colors), built lazily
        // once a GL context exists. Rotation is applied via a transform matrix.
        mutable Mesh m_globeMesh{};
        mutable Material m_globeMaterial{};
        mutable int m_uploadedSelectedCell = -1;
        int m_selectedLatitude = 0;
        int m_selectedLongitude = 0;
        float m_rotationDegrees = 0.0f;
        float m_tiltDegrees = -12.0f;
        float m_zoom = 1.0f;
        bool m_draggingGlobe = false;
        Vector2 m_lastMousePosition{};
    };
}
