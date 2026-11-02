#pragma once

#include "planet/StarSystem.h"
#include "planet/PlanetMap.h"
#include "raylib.h"

#include <vector>

namespace Infinitesia
{
    class PlanetViewer
    {
    public:
        explicit PlanetViewer(StarSystem system);

        void Update();
        void Draw() const;

    private:
        void HandleInput();
        Camera3D MakeCamera() const;

        StarSystem m_system;
        PlanetMap m_planetMap;
        std::vector<PlanetRegion> m_regions;
        std::vector<Color> m_regionColors;
        std::vector<float> m_regionBrightness;
        std::vector<Vector3> m_sphereVertices;
        int m_selectedLatitude = 0;
        int m_selectedLongitude = 0;
        float m_rotationDegrees = 0.0f;
        float m_tiltDegrees = -12.0f;
        float m_zoom = 1.0f;
        bool m_draggingGlobe = false;
        Vector2 m_lastMousePosition{};
    };
}
