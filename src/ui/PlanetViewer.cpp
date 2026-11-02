#include "ui/PlanetViewer.h"

#include "coordinates/PlanetaryCoordinates.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        constexpr float kPanelPadding = 24.0f;
        constexpr float kHeaderHeight = 82.0f;
        constexpr float kFooterHeight = 92.0f;
        constexpr float kCameraFieldOfView = 45.0f;
        constexpr float kPi = 3.14159265358979323846f;

        struct ViewerLayout
        {
            Rectangle mapPanel{};
            Rectangle globePanel{};
            Rectangle mapBounds{};
            Vector2 globeCenter{};
            float globeRadius = 0.0f;
        };

        ViewerLayout GetLayout(int screenWidth, int screenHeight, const PlanetMap& planetMap)
        {
            const float width = static_cast<float>(screenWidth);
            const float height = static_cast<float>(screenHeight);
            const float contentTop = kHeaderHeight;
            const float contentBottom = std::max(contentTop + 1.0f, height - kFooterHeight);
            const float contentHeight = contentBottom - contentTop;
            const float splitX = width * 0.48f;

            ViewerLayout layout;
            layout.mapPanel = {
                kPanelPadding,
                contentTop,
                std::max(1.0f, splitX - kPanelPadding * 1.5f),
                contentHeight
            };
            layout.globePanel = {
                splitX,
                contentTop,
                std::max(1.0f, width - splitX - kPanelPadding),
                contentHeight
            };

            const float mapWidth = layout.mapPanel.width - 36.0f;
            const float mapHeight = layout.mapPanel.height - 100.0f;
            const float mapAspectRatio =
                static_cast<float>(planetMap.GetLongitudeSegmentCount()) /
                static_cast<float>(planetMap.GetLatitudeBandCount());
            float fittedWidth = std::min(mapWidth, mapHeight * mapAspectRatio);
            float fittedHeight = fittedWidth / mapAspectRatio;
            if (fittedWidth <= 0.0f || fittedHeight <= 0.0f)
            {
                fittedWidth = std::max(1.0f, mapWidth);
                fittedHeight = std::max(1.0f, mapHeight);
            }
            layout.mapBounds = {
                layout.mapPanel.x + (layout.mapPanel.width - fittedWidth) * 0.5f,
                layout.mapPanel.y + (layout.mapPanel.height - fittedHeight) * 0.5f,
                fittedWidth,
                fittedHeight
            };

            layout.globeCenter = {
                layout.globePanel.x + layout.globePanel.width * 0.5f,
                layout.globePanel.y + layout.globePanel.height * 0.5f
            };
            layout.globeRadius = std::max(
                1.0f,
                std::min(layout.globePanel.width * 0.39f, layout.globePanel.height * 0.42f));
            return layout;
        }

        Color LerpColor(Color first, Color second, float amount)
        {
            const float t = std::clamp(amount, 0.0f, 1.0f);
            return {
                static_cast<unsigned char>(first.r + (second.r - first.r) * t),
                static_cast<unsigned char>(first.g + (second.g - first.g) * t),
                static_cast<unsigned char>(first.b + (second.b - first.b) * t),
                255
            };
        }

        Color GetRegionColor(const PlanetRegion& region)
        {
            if (region.isOcean)
            {
                const float depth = std::clamp((0.12f - static_cast<float>(region.elevationNormalized)) / 1.12f, 0.0f, 1.0f);
                return LerpColor({ 36, 155, 164, 255 }, { 13, 35, 83, 255 }, depth);
            }

            const float elevation = std::clamp(
                (static_cast<float>(region.elevationNormalized) - 0.12f) / 0.88f,
                0.0f,
                1.0f);
            const float temperature = static_cast<float>(region.temperatureCelsius);
            const float precipitation = static_cast<float>(region.precipitationMm);

            if (temperature < -5.0f || elevation > 0.82f)
            {
                return LerpColor({ 126, 137, 139, 255 }, { 238, 242, 238, 255 }, elevation);
            }
            if (temperature > 24.0f && precipitation < 650.0f)
            {
                return LerpColor({ 189, 154, 91, 255 }, { 137, 103, 65, 255 }, elevation);
            }
            if (precipitation < 430.0f)
            {
                return LerpColor({ 154, 151, 92, 255 }, { 112, 121, 82, 255 }, elevation);
            }
            return LerpColor({ 89, 143, 91, 255 }, { 91, 111, 86, 255 }, elevation);
        }

        Color ShadeColor(Color color, float brightness)
        {
            return {
                static_cast<unsigned char>(std::clamp(color.r * brightness, 0.0f, 255.0f)),
                static_cast<unsigned char>(std::clamp(color.g * brightness, 0.0f, 255.0f)),
                static_cast<unsigned char>(std::clamp(color.b * brightness, 0.0f, 255.0f)),
                color.a
            };
        }

        bool Contains(Rectangle rectangle, Vector2 point)
        {
            return point.x >= rectangle.x &&
                   point.x < rectangle.x + rectangle.width &&
                   point.y >= rectangle.y &&
                   point.y < rectangle.y + rectangle.height;
        }

        Vector3 GetSphereVertex(int row, int column, int rows, int columns)
        {
            const GeographicCoordinates coordinates{
                90.0 - static_cast<double>(row) / rows * 180.0,
                static_cast<double>(column) / columns * 360.0
            };
            const CartesianPoint3D point = PlanetaryCoordinates::ToCartesian(coordinates);
            return {
                static_cast<float>(point.x),
                static_cast<float>(point.y),
                static_cast<float>(point.z)
            };
        }

        void DrawTriangle(Vector3 first, Vector3 second, Vector3 third)
        {
            rlVertex3f(first.x, first.y, first.z);
            rlVertex3f(second.x, second.y, second.z);
            rlVertex3f(third.x, third.y, third.z);
        }
    }

    PlanetViewer::PlanetViewer(PlanetParameters parameters)
        : m_planetMap(std::move(parameters)),
          m_selectedLatitude(m_planetMap.GetLatitudeBandCount() / 2),
          m_selectedLongitude(m_planetMap.GetLongitudeSegmentCount() / 2)
    {
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();
        m_regions.reserve(static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns));
        m_regionColors.reserve(m_regions.capacity());
        m_regionBrightness.reserve(m_regions.capacity());
        const Vector3 lightDirection = Vector3Normalize(Vector3{ -0.45f, 0.62f, 0.85f });
        for (int latitude = 0; latitude < rows; ++latitude)
        {
            for (int longitude = 0; longitude < columns; ++longitude)
            {
                const PlanetRegion region = m_planetMap.GetRegion(latitude, longitude);
                m_regions.push_back(region);
                m_regionColors.push_back(GetRegionColor(region));

                const float latitudeRadians =
                    static_cast<float>(region.latitudeDegrees) * kPi / 180.0f;
                const float longitudeRadians =
                    static_cast<float>(region.longitudeDegrees) * kPi / 180.0f;
                const Vector3 normal{
                    std::cos(latitudeRadians) * std::cos(longitudeRadians),
                    std::sin(latitudeRadians),
                    std::cos(latitudeRadians) * std::sin(longitudeRadians)
                };
                m_regionBrightness.push_back(
                    0.24f + 0.76f * std::max(0.0f, Vector3DotProduct(normal, lightDirection)));
            }
        }

        m_sphereVertices.reserve(
            static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
        for (int latitude = 0; latitude <= rows; ++latitude)
        {
            for (int longitude = 0; longitude <= columns; ++longitude)
            {
                m_sphereVertices.push_back(GetSphereVertex(latitude, longitude, rows, columns));
            }
        }
    }

    void PlanetViewer::Update()
    {
        HandleInput();
        if (!m_draggingGlobe)
        {
            m_rotationDegrees = std::fmod(m_rotationDegrees + GetFrameTime() * 8.0f, 360.0f);
        }
    }

    Camera3D PlanetViewer::MakeCamera() const
    {
        const ViewerLayout layout = GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
        const float fieldOfViewRadians = kCameraFieldOfView * kPi / 180.0f;
        const float focalLength = static_cast<float>(GetScreenHeight()) /
                                  (2.0f * std::tan(fieldOfViewRadians * 0.5f));
        const float cameraDistance = std::max(
            2.4f,
            focalLength / layout.globeRadius * 1.08f) * m_zoom;
        const float targetX =
            (static_cast<float>(GetScreenWidth()) * 0.5f - layout.globeCenter.x) *
            cameraDistance / focalLength;
        const float targetY =
            (layout.globeCenter.y - static_cast<float>(GetScreenHeight()) * 0.5f) *
            cameraDistance / focalLength;

        Camera3D camera{};
        camera.position = { targetX, targetY, cameraDistance };
        camera.target = { targetX, targetY, 0.0f };
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = kCameraFieldOfView;
        camera.projection = CAMERA_PERSPECTIVE;
        return camera;
    }

    void PlanetViewer::HandleInput()
    {
        const Vector2 mousePosition = GetMousePosition();
        const ViewerLayout layout = GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (Contains(layout.mapBounds, mousePosition))
            {
                const float normalizedX = (mousePosition.x - layout.mapBounds.x) / layout.mapBounds.width;
                const float normalizedY = (mousePosition.y - layout.mapBounds.y) / layout.mapBounds.height;
                m_selectedLongitude = std::clamp(
                    static_cast<int>(normalizedX * columns),
                    0,
                    columns - 1);
                m_selectedLatitude = rows - 1 - std::clamp(
                    static_cast<int>(normalizedY * rows),
                    0,
                    rows - 1);
                m_draggingGlobe = false;
            }
            else
            {
                const Camera3D camera = MakeCamera();
                const Ray ray = GetMouseRay(mousePosition, camera);
                const RayCollision collision = GetRayCollisionSphere(ray, { 0.0f, 0.0f, 0.0f }, 1.0f);
                m_draggingGlobe = collision.hit;
                m_lastMousePosition = mousePosition;

                if (collision.hit)
                {
                    const float yaw = -m_rotationDegrees * kPi / 180.0f;
                    const float pitch = -m_tiltDegrees * kPi / 180.0f;
                    const float cosYaw = std::cos(yaw);
                    const float sinYaw = std::sin(yaw);
                    const float xAfterYaw = cosYaw * collision.point.x + sinYaw * collision.point.z;
                    const float zAfterYaw = -sinYaw * collision.point.x + cosYaw * collision.point.z;
                    const float cosPitch = std::cos(pitch);
                    const float sinPitch = std::sin(pitch);
                    const float localY = cosPitch * collision.point.y - sinPitch * zAfterYaw;
                    const float localZ = sinPitch * collision.point.y + cosPitch * zAfterYaw;
                    const GeographicCoordinates coordinates = PlanetaryCoordinates::ToGeographic({
                        xAfterYaw,
                        localY,
                        localZ
                    });
                    m_selectedLatitude = std::clamp(
                        static_cast<int>((coordinates.latitudeDegrees + 90.0) / 180.0 * rows),
                        0,
                        rows - 1);
                    m_selectedLongitude = std::clamp(
                        static_cast<int>(coordinates.longitudeDegrees / 360.0 * columns),
                        0,
                        columns - 1);
                }
            }
        }

        if (m_draggingGlobe && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            const Vector2 delta{
                mousePosition.x - m_lastMousePosition.x,
                mousePosition.y - m_lastMousePosition.y
            };
            m_rotationDegrees = std::fmod(m_rotationDegrees + delta.x * 0.35f, 360.0f);
            m_tiltDegrees = std::clamp(m_tiltDegrees + delta.y * 0.35f, -75.0f, 75.0f);
            m_lastMousePosition = mousePosition;
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            m_draggingGlobe = false;
        }

        m_zoom = std::clamp(m_zoom - GetMouseWheelMove() * 0.1f, 0.55f, 2.0f);
    }

    void PlanetViewer::Draw() const
    {
        const int screenWidth = GetScreenWidth();
        const int screenHeight = GetScreenHeight();
        const ViewerLayout layout = GetLayout(screenWidth, screenHeight, m_planetMap);
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();

        ClearBackground({ 11, 17, 26, 255 });

        DrawText("INFINITESIA  /  PLANET VIEWER", 28, 22, 24, RAYWHITE);
        DrawText("Global map and interactive globe", 30, 52, 16, { 139, 158, 177, 255 });
        DrawFPS(screenWidth - 80, 24);

        DrawRectangleRounded(layout.mapPanel, 0.035f, 8, { 19, 28, 40, 255 });
        DrawRectangleRounded(layout.globePanel, 0.035f, 8, { 19, 28, 40, 255 });
        DrawText("SURFACE MAP", static_cast<int>(layout.mapPanel.x + 18.0f), static_cast<int>(layout.mapPanel.y + 16.0f), 16, SKYBLUE);
        DrawText("ROTATING GLOBE", static_cast<int>(layout.globePanel.x + 18.0f), static_cast<int>(layout.globePanel.y + 16.0f), 16, SKYBLUE);

        const float cellWidth = layout.mapBounds.width / static_cast<float>(columns);
        const float cellHeight = layout.mapBounds.height / static_cast<float>(rows);
        for (int latitude = 0; latitude < rows; ++latitude)
        {
            for (int longitude = 0; longitude < columns; ++longitude)
            {
                const int planetLatitude = rows - 1 - latitude;
                const std::size_t regionIndex =
                    static_cast<std::size_t>(planetLatitude) * columns + longitude;
                const Color color = planetLatitude == m_selectedLatitude &&
                                    longitude == m_selectedLongitude
                    ? Color{ 255, 202, 87, 255 }
                    : m_regionColors[regionIndex];
                const int left = static_cast<int>(std::floor(
                    layout.mapBounds.x + longitude * cellWidth));
                const int top = static_cast<int>(std::floor(
                    layout.mapBounds.y + latitude * cellHeight));
                const int right = static_cast<int>(std::floor(
                    layout.mapBounds.x + (longitude + 1) * cellWidth));
                const int bottom = static_cast<int>(std::floor(
                    layout.mapBounds.y + (latitude + 1) * cellHeight));
                DrawRectangle(
                    left,
                    top,
                    std::max(1, right - left),
                    std::max(1, bottom - top),
                    color);
            }
        }

        for (int longitude = 0; longitude <= 12; ++longitude)
        {
            const float x = layout.mapBounds.x + layout.mapBounds.width * longitude / 12.0f;
            DrawLine(
                static_cast<int>(x),
                static_cast<int>(layout.mapBounds.y),
                static_cast<int>(x),
                static_cast<int>(layout.mapBounds.y + layout.mapBounds.height),
                Fade(RAYWHITE, 0.12f));
        }
        for (int latitude = 0; latitude <= 6; ++latitude)
        {
            const float y = layout.mapBounds.y + layout.mapBounds.height * latitude / 6.0f;
            DrawLine(
                static_cast<int>(layout.mapBounds.x),
                static_cast<int>(y),
                static_cast<int>(layout.mapBounds.x + layout.mapBounds.width),
                static_cast<int>(y),
                Fade(RAYWHITE, 0.12f));
        }
        DrawRectangleLinesEx(layout.mapBounds, 1.0f, { 129, 158, 180, 255 });

        const Camera3D camera = MakeCamera();
        BeginMode3D(camera);
        rlPushMatrix();
        rlRotatef(m_rotationDegrees, 0.0f, 1.0f, 0.0f);
        rlRotatef(m_tiltDegrees, 1.0f, 0.0f, 0.0f);
        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        rlColor4ub(255, 255, 255, 255);

        for (int latitude = 0; latitude < rows; ++latitude)
        {
            for (int longitude = 0; longitude < columns; ++longitude)
            {
                const int planetLatitude = rows - 1 - latitude;
                const std::size_t regionIndex =
                    static_cast<std::size_t>(planetLatitude) * columns + longitude;
                const bool selected =
                    planetLatitude == m_selectedLatitude &&
                    longitude == m_selectedLongitude;
                const Color baseColor = selected
                    ? Color{ 255, 202, 87, 255 }
                    : m_regionColors[regionIndex];
                const Color shadedColor = ShadeColor(baseColor, m_regionBrightness[regionIndex]);
                rlColor4ub(shadedColor.r, shadedColor.g, shadedColor.b, 255);

                const std::size_t topLeftIndex =
                    static_cast<std::size_t>(latitude) * (columns + 1) + longitude;
                const Vector3 topLeft = m_sphereVertices[topLeftIndex];
                const Vector3 topRight = m_sphereVertices[topLeftIndex + 1];
                const Vector3 bottomLeft = m_sphereVertices[topLeftIndex + columns + 1];
                const Vector3 bottomRight = m_sphereVertices[topLeftIndex + columns + 2];
                DrawTriangle(topLeft, topRight, bottomLeft);
                DrawTriangle(topRight, bottomRight, bottomLeft);
            }
        }

        rlEnd();
        rlSetTexture(1);
        rlPopMatrix();
        EndMode3D();

        char selectionText[160];
        const PlanetRegion& selectedRegion =
            m_regions[static_cast<std::size_t>(m_selectedLatitude) * columns + m_selectedLongitude];
        std::snprintf(
            selectionText,
            sizeof(selectionText),
            "Region %d, %d  |  %.1f deg, %.1f deg  |  Elevation %.2f (normalized)  |  %.1f C",
            m_selectedLatitude,
            m_selectedLongitude,
            selectedRegion.latitudeDegrees,
            selectedRegion.longitudeDegrees,
            selectedRegion.elevationNormalized,
            selectedRegion.temperatureCelsius);

        const int footerY = screenHeight - 70;
        DrawText(selectionText, 30, footerY, 16, RAYWHITE);
        DrawText(
            "Click the map or globe to select a region  |  Drag to rotate  |  Scroll to zoom",
            30,
            footerY + 27,
            14,
            { 139, 158, 177, 255 });
    }
}
