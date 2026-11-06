#include "app/Application.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/biology/community/VegetationLayer.h"
#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/planetary/PlanetLoadSteps.h"
#include "domains/natural/planetary/StarSystemGenerator.h"
#include "raylib.h"
#include "world/PlanetTerrainSource.h"

namespace Infinitesia
{
namespace
{
// Per-step durations of the last load, in the working directory.
constexpr const char *kLoadProfilePath = "loading_profile.txt";

// The items and object kinds ship next to the executable.
std::shared_ptr<const GameContent> LoadGameContent()
{
	return GameContent::LoadFromDirectory(std::filesystem::path(GetApplicationDirectory()) / "assets" / "gameplay");
}
} // namespace

Application::Application(ApplicationConfig config)
    : m_config(std::move(config)), m_content(LoadGameContent()),
      m_campaign(m_config.simulationTicksPerSecond), m_worldRenderer(m_config.camera),
      m_cameraController(m_config.cameraControls), m_showPathDebug(m_config.showPathDebug)
{
	if (m_config.camera.distance < m_config.cameraControls.minimumDistance ||
	    m_config.camera.distance > m_config.cameraControls.maximumDistance)
	{
		throw std::invalid_argument("Initial camera settings must fit within camera control limits.");
	}
}

void Application::StartSettlement(const Site &site)
{
	// The data model supports several sites, but the game currently has one.
	// Keep the list and the site entry points for later outposts; the player
	// gets a single settlement.
	if (!m_sites.empty())
	{
		return;
	}
	Site founded = site;
	if (founded.id == kNoSite)
	{
		founded.id = m_nextSiteId++;
	}
	else
	{
		m_nextSiteId = std::max(m_nextSiteId, founded.id + 1);
	}
	m_sites.push_back(SiteRuntime{founded, nullptr});
	EnterSite(founded.id);
	if (m_planetViewer != nullptr)
	{
		m_planetViewer->SetFoundingEnabled(false);
	}
}

void Application::EnterSite(SiteId id)
{
	const auto found = std::find_if(m_sites.begin(), m_sites.end(),
	                                [id](const SiteRuntime &runtime) { return runtime.site.id == id; });
	if (found == m_sites.end())
	{
		return;
	}
	if (found->game == nullptr)
	{
		CreateLocalGame(*found);
	}
	if (found->game == nullptr)
	{
		return;
	}

	// Only the site being played keeps its work and fluid systems enabled.
	if (m_activeSite != kNoSite && m_activeSite != id)
	{
		if (LocalGame *previous = ActiveGame())
		{
			previous->SetEnabled(false);
		}
	}
	found->game->SetEnabled(true);
	m_activeSite = id;
	// A selection from the previous site points into that site's object store.
	m_worldInteraction = WorldInteractionController{};
	const GridCell start = found->game->GetStartingCells().worker;
	const int level = start.y;
	m_selectedLevel = level;
	m_chunkViewCenter = start;

	// Look at the middle of the world from the usual distance.
	Camera3D &camera = m_worldRenderer.GetCamera();
	const Grid &grid = found->game->GetGrid();
	camera.target.y = static_cast<float>(level + 1) * grid.GetCellSize();
	m_cameraController.SetWorldHalfExtents(grid.GetWorldWidth() * 0.5f, grid.GetWorldDepth() * 0.5f);
	m_cameraController.Reset(camera, 0.0f, 0.0f, m_config.camera.distance);

	m_screen = Screen::Local;
}

void Application::LeaveSite()
{
	if (LocalGame *game = ActiveGame())
	{
		game->SetEnabled(false);
	}
	m_screen = Screen::Planet;
}

void Application::CreateLocalGame(SiteRuntime &runtime)
{
	if (m_planetViewer == nullptr)
	{
		return;
	}
	const WorldTerrain &terrain = m_planetViewer->GetRegionTerrain();
	const Site &site = runtime.site;
	double geologyEast = 0.0;
	double geologyNorth = 0.0;
	site.grid.Project(PlanetaryCoordinates::ToCartesian(site.center), geologyEast, geologyNorth);
	auto settlement = std::make_shared<const SettlementTerrain>(terrain, site.frame, geologyEast, geologyNorth);

	// Materials, geology columns and shapes come from the assets shipped next to
	// the executable.
	const std::filesystem::path assets = std::filesystem::path(GetApplicationDirectory()) / "assets";
	auto materials =
	    m_matter != nullptr
	        ? m_matter
	        : std::make_shared<const MatterCatalog>(MatterCatalog::LoadFromFile(assets / "matter" / "matter.xml"));
	auto catalog = std::make_shared<const GeologyCatalog>(
	    GeologyCatalog::Load(materials, assets / "geology" / "province_columns.json"));
	auto registry =
	    std::make_shared<const BlockRegistry>(BlockRegistry::Build(materials, assets / "blocks" / "shapes.json"));
	auto geology = std::make_shared<const GeologyModel>(catalog, terrain.GetPlanetMap());
	auto flora = m_species != nullptr ? m_species
	                                  : std::make_shared<const SpeciesCatalog>(
	                                        SpeciesCatalog::Load(assets / "biology" / "species.json", *materials));

	const GridConfig gridConfig = m_config.settlementGrid;
	auto source = std::make_shared<PlanetTerrainSource>(settlement, gridConfig.width, gridConfig.height,
	                                                    gridConfig.depth, m_config.settlementAnchorLevel, registry,
	                                                    geology, flora, m_config.worldSeed);
	const std::string systemPrefix = "site." + std::to_string(runtime.site.id);
	runtime.game =
	    std::make_unique<LocalGame>(m_content, LocalGameConfig{gridConfig, m_config.worker}, source,
	                               m_campaign.Runtime(), systemPrefix);
	// The terrain caches are filled by one worker while the main thread draws.
	runtime.game->GetWorld().terrain.SetBackgroundGeneration(true);
}

LocalGame *Application::ActiveGame()
{
	const auto found = std::find_if(m_sites.begin(), m_sites.end(),
	                                [this](const SiteRuntime &runtime) { return runtime.site.id == m_activeSite; });
	return found != m_sites.end() ? found->game.get() : nullptr;
}

const LocalGame *Application::ActiveGame() const
{
	const auto found = std::find_if(m_sites.begin(), m_sites.end(),
	                                [this](const SiteRuntime &runtime) { return runtime.site.id == m_activeSite; });
	return found != m_sites.end() ? found->game.get() : nullptr;
}

void Application::StartPlanetLoad()
{
	// Generation and PlanetViewer construction are CPU-only (no GL calls),
	// so they are safe to run off the main thread.
	const std::uint64_t seed = m_config.worldSeed;
	LoadProgress *progress = &m_loadProgress;
	// Step shares come from the durations measured on the previous run; the
	// equal-weight default plan only applies when no profile exists yet.
	progress->SetDefaultPlan({LoadStep::kStarSystem, LoadStep::kTectonics, LoadStep::kTerrain, LoadStep::kGeology,
	                          LoadStep::kClimateContinentality, LoadStep::kClimateTemperature,
	                          LoadStep::kClimatePrecipitation, LoadStep::kHydrology, LoadStep::kSoils,
	                          LoadStep::kBiomes, LoadStep::kVegetation, LoadStep::kViewerColors,
	                          LoadStep::kViewerSmoothing});
	progress->LoadProfile(kLoadProfilePath);

	// The planet grows its vegetation from the species catalog, which the
	// settlement reads again later.
	const std::filesystem::path assets = std::filesystem::path(GetApplicationDirectory()) / "assets";
	m_matter = std::make_shared<const MatterCatalog>(MatterCatalog::LoadFromFile(assets / "matter" / "matter.xml"));
	m_species =
	    std::make_shared<const SpeciesCatalog>(SpeciesCatalog::Load(assets / "biology" / "species.json", *m_matter));
	PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
	AddVegetationStages(pipeline, m_species);

	m_planetLoad = std::async(std::launch::async, [seed, progress, pipeline = std::move(pipeline)]() {
		BeginLoadStep(progress, LoadStep::kStarSystem, "Generating star system");
		StarSystem system = StarSystemGenerator::Generate(GenerationScenario{.seed = seed});
		auto viewer = std::make_unique<PlanetViewer>(std::move(system), pipeline, progress);
		progress->Finish();
		progress->SaveProfile(kLoadProfilePath); // best effort; a failed save
		                                         // only skips recalibration
		return viewer;
	});
}

void Application::PollPlanetLoad()
{
	if (!m_planetLoad.valid() || m_planetLoad.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
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
	DrawRectangle(barX, centerY - barHeight / 2, barWidth, barHeight, Color{40, 40, 48, 255});
	DrawRectangle(barX, centerY - barHeight / 2, static_cast<int>(barWidth * fraction), barHeight,
	              Color{255, 202, 87, 255});
	DrawRectangleLines(barX, centerY - barHeight / 2, barWidth, barHeight, GRAY);
	const char *percent = TextFormat("%d%%", static_cast<int>(fraction * 100.0f));
	DrawText(percent, barX, centerY + barHeight, 18, GRAY);
}
} // namespace Infinitesia
