#include "app/Application.h"
#include "raylib.h"
#include "gameplay/commands/ChopTreeCommand.h"
#include "gameplay/commands/PickUpItemCommand.h"
#include "geology/model/GeologyModel.h"
#include "planet/PlanetLoadSteps.h"
#include "planet/StarSystemGenerator.h"
#include "world/PlanetTerrainSource.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        // Per-step durations of the last load, in the working directory.
        constexpr const char* kLoadProfilePath = "loading_profile.txt";
    }

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_grid(m_config.grid),
          m_world(m_grid, m_config.initialWorldObjects, m_config.worldSeed),
          m_simulation(m_config.simulationTicksPerSecond),
          m_workSystem(m_config.worker),
          m_worldRenderer(m_config.camera),
          m_cameraController(m_config.cameraControls),
          m_chunkViewCenter{
              m_config.grid.width / 2,
              m_config.grid.height - 1,
              m_config.grid.depth / 2
          },
          m_selectedLevel(m_config.grid.height - 1),
          m_showPathDebug(m_config.showPathDebug)
    {
        Camera3D& camera = m_worldRenderer.GetCamera();
        camera.position.y += m_grid.GetWorldHeight();
        camera.target.y = m_grid.GetWorldHeight();

        m_commands.Register(std::make_unique<ChopTreeCommand>(m_workSystem, m_grid));
        m_commands.Register(std::make_unique<PickUpItemCommand>());

        if (m_config.camera.distance < m_config.cameraControls.minimumDistance ||
            m_config.camera.distance > m_config.cameraControls.maximumDistance ||
            m_config.camera.elevationDegrees < m_config.cameraControls.minimumElevationDegrees ||
            m_config.camera.elevationDegrees > m_config.cameraControls.maximumElevationDegrees)
        {
            throw std::invalid_argument("Initial camera settings must fit within camera control limits.");
        }
    }

    void Application::StartSettlement(const SettlementSite& site)
    {
        const RegionTerrain& terrain = m_planetViewer->GetRegionTerrain();
        auto settlement = std::make_shared<const SettlementTerrain>(
            terrain, site.grid,
            site.region.i * LocalGrid::kRegionSizeMeters,
            site.region.j * LocalGrid::kRegionSizeMeters);

        // The rocks under the soil come from the geology catalog shipped next to the executable.
        auto catalog = std::make_shared<const GeologyCatalog>(GeologyCatalog::LoadFromDirectory(
            std::filesystem::path(GetApplicationDirectory()) / "assets" / "geology"));
        auto geology = std::make_shared<const GeologyModel>(catalog, terrain.GetPlanetMap());

        const GridConfig gridConfig = m_config.settlementGrid;
        auto source = std::make_shared<PlanetTerrainSource>(
            settlement, gridConfig.width, gridConfig.height, gridConfig.depth, m_config.settlementAnchorLevel,
            geology);

        // The worker starts on the ground of the middle column, and a tree on the same
        // level close by. Movement is still by level (a cell is walkable where there is
        // a block on the worker's level), so the tree has to be reachable: every cell of
        // the way there, east first and then south, must have ground at that level or higher.
        const int centerX = gridConfig.width / 2;
        const int centerZ = gridConfig.depth / 2;
        const int level = source->SurfaceLevelAt(centerX, centerZ);
        const auto reachable = [&](int dx, int dz)
        {
            for (int step = 0; step <= std::abs(dx); ++step)
            {
                if (source->SurfaceLevelAt(centerX + (dx < 0 ? -step : step), centerZ) < level)
                {
                    return false;
                }
            }
            for (int step = 0; step <= std::abs(dz); ++step)
            {
                if (source->SurfaceLevelAt(centerX + dx, centerZ + (dz < 0 ? -step : step)) < level)
                {
                    return false;
                }
            }
            return true;
        };
        std::vector<WorldObjectPlacement> objects{
            { WorldObjectType::Worker, { centerX, level, centerZ } }
        };
        for (int radius = 3; radius <= 16 && objects.size() < 2; ++radius)
        {
            for (int dz = -radius; dz <= radius && objects.size() < 2; ++dz)
            {
                for (int dx = -radius; dx <= radius && objects.size() < 2; ++dx)
                {
                    if (std::max(std::abs(dx), std::abs(dz)) == radius &&
                        source->SurfaceLevelAt(centerX + dx, centerZ + dz) == level && reachable(dx, dz))
                    {
                        objects.push_back({ WorldObjectType::Tree, { centerX + dx, level, centerZ + dz } });
                    }
                }
            }
        }

        m_grid = Grid(gridConfig);
        m_world = GameWorld(m_grid, objects, source);
        m_workSystem = WorkSystem(m_config.worker);

        m_selectedLevel = level;
        m_chunkViewCenter = { centerX, level, centerZ };

        // Look at the middle of the world from the usual distance and angle.
        Camera3D& camera = m_worldRenderer.GetCamera();
        const float elevation = m_config.camera.elevationDegrees * PI / 180.0f;
        const float targetY = static_cast<float>(level + 1) * m_grid.GetCellSize();
        camera.target = { 0.0f, targetY, 0.0f };
        camera.position = {
            0.0f,
            targetY + m_config.camera.distance * std::sin(elevation),
            m_config.camera.distance * std::cos(elevation)
        };

        m_settlementFounded = true;
        m_planetViewer->SetFoundingEnabled(false);
        m_screen = Screen::Local;
    }

    void Application::StartPlanetLoad()
    {
        // Generation and PlanetViewer construction are CPU-only (no GL calls),
        // so they are safe to run off the main thread.
        const std::uint64_t seed = m_config.worldSeed;
        LoadProgress* progress = &m_loadProgress;
        // Step shares come from the durations measured on the previous run; the
        // equal-weight default plan only applies when no profile exists yet.
        progress->SetDefaultPlan({
            LoadStep::kStarSystem, LoadStep::kTectonics, LoadStep::kTerrain, LoadStep::kGeology,
            LoadStep::kClimateContinentality, LoadStep::kClimateTemperature,
            LoadStep::kClimatePrecipitation, LoadStep::kHydrology, LoadStep::kSoils,
            LoadStep::kBiomes, LoadStep::kViewerColors, LoadStep::kViewerSmoothing });
        progress->LoadProfile(kLoadProfilePath);
        m_planetLoad = std::async(std::launch::async, [seed, progress]() {
            BeginLoadStep(progress, LoadStep::kStarSystem, "Generating star system");
            StarSystem system = StarSystemGenerator::Generate(GenerationScenario{ .seed = seed });
            auto viewer = std::make_unique<PlanetViewer>(std::move(system), progress);
            progress->Finish();
            progress->SaveProfile(kLoadProfilePath); // best effort; a failed save only skips recalibration
            return viewer;
        });
    }

    void Application::PollPlanetLoad()
    {
        if (!m_planetLoad.valid() ||
            m_planetLoad.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }
        m_planetViewer = m_planetLoad.get(); // rethrows generation errors on the main thread
    }

    void Application::DrawLoadingScreen() const
    {
        ClearBackground(BLACK);
        const float fraction = std::clamp(m_loadProgress.GetFraction(), 0.0f, 1.0f);
        const std::string label = m_loadProgress.GetLabel() + "...";

        const int screenWidth = GetScreenWidth();
        const int centerY = GetScreenHeight() / 2;
        const int barWidth = std::min(480, screenWidth - 80);
        const int barHeight = 14;
        const int barX = (screenWidth - barWidth) / 2;

        const int fontSize = 24;
        DrawText(label.c_str(), barX, centerY - barHeight - fontSize - 8, fontSize, RAYWHITE);
        DrawRectangle(barX, centerY - barHeight / 2, barWidth, barHeight, Color{ 40, 40, 48, 255 });
        DrawRectangle(barX, centerY - barHeight / 2, static_cast<int>(barWidth * fraction),
                      barHeight, Color{ 255, 202, 87, 255 });
        DrawRectangleLines(barX, centerY - barHeight / 2, barWidth, barHeight, GRAY);
        const char* percent = TextFormat("%d%%", static_cast<int>(fraction * 100.0f));
        DrawText(percent, barX, centerY + barHeight, 18, GRAY);
    }
}
