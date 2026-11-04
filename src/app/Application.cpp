#include "app/Application.h"
#include "raylib.h"
#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/community/VegetationLayer.h"
#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/planetary/PlanetLoadSteps.h"
#include "domains/natural/planetary/StarSystemGenerator.h"
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

        // The items and object kinds ship next to the executable.
        std::shared_ptr<const GameContent> LoadGameContent()
        {
            return GameContent::LoadFromDirectory(
                std::filesystem::path(GetApplicationDirectory()) / "assets" / "gameplay");
        }
    }

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_content(LoadGameContent()),
          m_worldRenderer(m_config.camera),
          m_cameraController(m_config.cameraControls),
          m_showPathDebug(m_config.showPathDebug)
    {
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

        // Materials, geology columns and shapes come from the assets shipped next to the executable.
        const std::filesystem::path assets = std::filesystem::path(GetApplicationDirectory()) / "assets";
        auto materials = m_materials != nullptr ? m_materials
            : std::make_shared<const MaterialCatalog>(MaterialCatalog::LoadFromFile(assets / "materials" / "materials.json"));
        auto catalog = std::make_shared<const GeologyCatalog>(
            GeologyCatalog::Load(materials, assets / "geology" / "province_columns.json"));
        auto registry = std::make_shared<const BlockRegistry>(
            BlockRegistry::Build(materials, assets / "blocks" / "shapes.json"));
        auto geology = std::make_shared<const GeologyModel>(catalog, terrain.GetPlanetMap());
        auto flora = m_species != nullptr ? m_species
            : std::make_shared<const SpeciesCatalog>(SpeciesCatalog::Load(assets / "biology" / "species.json", *materials));

        const GridConfig gridConfig = m_config.settlementGrid;
        auto source = std::make_shared<PlanetTerrainSource>(
            settlement, gridConfig.width, gridConfig.height, gridConfig.depth, m_config.settlementAnchorLevel,
            registry, geology, flora, m_config.worldSeed);

        m_game.reset();
        m_game.emplace(m_content,
                       LocalGameConfig{ gridConfig, m_config.worker, m_config.simulationTicksPerSecond },
                       source);

        const GridCell start = m_game->GetStartingCells().worker;
        const int level = start.y;
        m_selectedLevel = level;
        m_chunkViewCenter = start;

        // Look at the middle of the world from the usual distance and angle.
        Camera3D& camera = m_worldRenderer.GetCamera();
        const float elevation = m_config.camera.elevationDegrees * PI / 180.0f;
        const float targetY = static_cast<float>(level + 1) * m_game->GetGrid().GetCellSize();
        camera.target = { 0.0f, targetY, 0.0f };
        camera.position = {
            0.0f,
            targetY + m_config.camera.distance * std::sin(elevation),
            m_config.camera.distance * std::cos(elevation)
        };

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
            LoadStep::kBiomes, LoadStep::kVegetation, LoadStep::kViewerColors, LoadStep::kViewerSmoothing });
        progress->LoadProfile(kLoadProfilePath);

        // The planet grows its vegetation from the species catalog, which the settlement reads again later.
        const std::filesystem::path assets = std::filesystem::path(GetApplicationDirectory()) / "assets";
        m_materials = std::make_shared<const MaterialCatalog>(
            MaterialCatalog::LoadFromFile(assets / "materials" / "materials.json"));
        m_species = std::make_shared<const SpeciesCatalog>(
            SpeciesCatalog::Load(assets / "biology" / "species.json", *m_materials));
        PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
        AddVegetationStages(pipeline, m_species);

        m_planetLoad = std::async(std::launch::async, [seed, progress, pipeline = std::move(pipeline)]() {
            BeginLoadStep(progress, LoadStep::kStarSystem, "Generating star system");
            StarSystem system = StarSystemGenerator::Generate(GenerationScenario{ .seed = seed });
            auto viewer = std::make_unique<PlanetViewer>(std::move(system), pipeline, progress);
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
