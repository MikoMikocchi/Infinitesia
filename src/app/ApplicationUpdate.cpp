#include "app/Application.h"
#include "app/internal/AppHelpers.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr float kDegreesToRadians = PI / 180.0f;
        constexpr std::size_t kChunksGeneratedPerFrame = 4;

        GridCell GetCameraGridCell(const Grid& grid, Vector3 cameraTarget, int selectedLevel)
        {
            const float halfWidth = grid.GetWorldWidth() * 0.5f;
            const float halfDepth = grid.GetWorldDepth() * 0.5f;
            const float edgeInset = grid.GetCellSize() * 0.001f;
            const WorldPosition target{
                std::clamp(cameraTarget.x, -halfWidth + edgeInset, halfWidth - edgeInset),
                static_cast<float>(selectedLevel + 1) * grid.GetCellSize(),
                std::clamp(cameraTarget.z, -halfDepth + edgeInset, halfDepth - edgeInset)
            };
            const std::optional<GridCell> cell = grid.WorldToCell(target);
            if (!cell.has_value())
            {
                throw std::logic_error("Clamped camera target must map to a grid cell.");
            }
            return *cell;
        }

        int GetChunkRenderRadius(const Camera3D& camera, const Grid& grid)
        {
            const float offsetX = camera.position.x - camera.target.x;
            const float offsetY = camera.position.y - camera.target.y;
            const float offsetZ = camera.position.z - camera.target.z;
            const float cameraDistance = std::sqrt(
                offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
            const float aspectRatio = static_cast<float>(GetScreenWidth()) /
                                      static_cast<float>(GetScreenHeight());
            const float visibleHalfWidth =
                cameraDistance * std::tan(camera.fovy * kDegreesToRadians * 0.5f) * aspectRatio;
            const float chunkWorldSize = static_cast<float>(kChunkSize) * grid.GetCellSize();
            return static_cast<int>(std::ceil(visibleHalfWidth / chunkWorldSize)) + 2;
        }
    }

    // Development hooks, driven by environment variables so that the local world can be
    // checked without clicking through the planet viewer:
    //   INFINITESIA_DEV_FOUND="lat,lon"      found the settlement at that place on startup
    //   INFINITESIA_DEV_SCREENSHOT=name.png  save a screenshot of the local world, then quit
    void Application::UpdateDevHooks()
    {
        static const char* const found = std::getenv("INFINITESIA_DEV_FOUND");
        static const char* const screenshot = std::getenv("INFINITESIA_DEV_SCREENSHOT");

        double latitude = 0.0;
        double longitude = 0.0;
        if (found != nullptr && m_planetViewer && !m_settlementFounded &&
            std::sscanf(found, "%lf,%lf", &latitude, &longitude) == 2)
        {
            const RegionTerrain& terrain = m_planetViewer->GetRegionTerrain();
            const double radius = terrain.GetPlanetMap().GetPlanet().GetRadius();
            StartSettlement({ LocalGrid(radius, PlanetaryCoordinates::ToCartesian({ latitude, longitude })), RegionId{} });
        }

        if (screenshot != nullptr && m_screen == Screen::Local && ++m_devFramesInLocalScreen == 150)
        {
            TakeScreenshot(screenshot);
            m_exitRequested = true;
        }
    }

    void Application::Update()
    {
        PollPlanetLoad();
        UpdateDevHooks();

        if (m_screen == Screen::Local)
        {
            if (IsKeyPressed(KEY_M))
            {
                m_screen = Screen::Planet; // back to the planet map
                return;
            }
            UpdateLocal();
            return;
        }

        if (!m_planetViewer)
        {
            return;
        }
        if (m_settlementFounded && IsKeyPressed(KEY_M))
        {
            m_screen = Screen::Local;
            return;
        }
        m_planetViewer->Update();
        if (const std::optional<SettlementSite> site = m_planetViewer->TakeFoundedSite())
        {
            StartSettlement(*site);
        }
    }

    void Application::UpdateLocal()
    {
        if (IsKeyPressed(KEY_F3))
        {
            m_showPathDebug = !m_showPathDebug;
        }

        AppInternal::UpdateLevelSelection(m_worldRenderer, m_grid, m_selectedLevel);

        m_cameraController.Update(m_worldRenderer.GetCamera(), m_time.GetDeltaTime());
        m_chunkViewCenter = GetCameraGridCell(
            m_grid,
            m_worldRenderer.GetCamera().target,
            m_selectedLevel);
        m_chunkViewCenter.y = m_selectedLevel;
        m_chunkRenderRadius = GetChunkRenderRadius(m_worldRenderer.GetCamera(), m_grid);
        // Empty cells look up to 16 levels down for ground, which can lie in the chunk below.
        const int verticalChunkRadius = 1;
        m_world.terrain.RequestChunksAround(
            m_chunkViewCenter,
            m_chunkRenderRadius,
            verticalChunkRadius);
        m_world.terrain.GeneratePendingChunks(kChunksGeneratedPerFrame);
        m_worldInteraction.Update(
            {
                GetMousePosition(),
                IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                IsKeyPressed(KEY_ESCAPE)
            },
            m_grid,
            m_world,
            m_workSystem,
            m_worldRenderer.GetCamera(),
            m_commands,
            m_config.window.width,
            m_config.window.height,
            m_selectedLevel);
    }
}
