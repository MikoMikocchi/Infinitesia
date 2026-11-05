#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "app/Application.h"
#include "app/internal/AppHelpers.h"
#include "raylib.h"

namespace Infinitesia
{
namespace
{
constexpr float kDegreesToRadians = PI / 180.0f;
constexpr std::size_t kChunksGeneratedPerFrame = 4;
constexpr int kVerticalChunkRadius = 4;

GridCell GetCameraGridCell(const Grid &grid, Vector3 cameraTarget, int selectedLevel)
{
	const float halfWidth = grid.GetWorldWidth() * 0.5f;
	const float halfDepth = grid.GetWorldDepth() * 0.5f;
	const float edgeInset = grid.GetCellSize() * 0.001f;
	const WorldPosition target{std::clamp(cameraTarget.x, -halfWidth + edgeInset, halfWidth - edgeInset),
	                           static_cast<float>(selectedLevel + 1) * grid.GetCellSize(),
	                           std::clamp(cameraTarget.z, -halfDepth + edgeInset, halfDepth - edgeInset)};
	const std::optional<GridCell> cell = grid.WorldToCell(target);
	if (!cell.has_value())
	{
		throw std::logic_error("Clamped camera target must map to a grid cell.");
	}
	return *cell;
}

int GetChunkRenderRadius(const Camera3D &camera, const Grid &grid)
{
	const float offsetX = camera.position.x - camera.target.x;
	const float offsetY = camera.position.y - camera.target.y;
	const float offsetZ = camera.position.z - camera.target.z;
	const float cameraDistance = std::sqrt(offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
	const float aspectRatio = static_cast<float>(GetScreenWidth()) / static_cast<float>(GetScreenHeight());
	const float visibleHalfWidth = cameraDistance * std::tan(camera.fovy * kDegreesToRadians * 0.5f) * aspectRatio;
	const float chunkWorldSize = static_cast<float>(kChunkSize) * grid.GetCellSize();
	return static_cast<int>(std::ceil(visibleHalfWidth / chunkWorldSize)) + 2;
}
} // namespace

// Development hooks, driven by environment variables so that the local world
// can be checked without clicking through the planet viewer:
//   INFINITESIA_DEV_FOUND="lat,lon"      found the settlement at that place on
//   startup INFINITESIA_DEV_SCREENSHOT=name.png  save a screenshot of the local
//   world, then quit INFINITESIA_DEV_VIEW=all             show the whole world,
//   not only the part below the selected level
void Application::UpdateDevHooks()
{
	static const char *const found = std::getenv("INFINITESIA_DEV_FOUND");
	static const char *const screenshot = std::getenv("INFINITESIA_DEV_SCREENSHOT");
	static const char *const view = std::getenv("INFINITESIA_DEV_VIEW");

	double latitude = 0.0;
	double longitude = 0.0;
	if (found != nullptr && m_planetViewer && m_sites.empty() &&
	    std::sscanf(found, "%lf,%lf", &latitude, &longitude) == 2)
	{
		const WorldTerrain &terrain = m_planetViewer->GetRegionTerrain();
		const double radius = terrain.GetPlanetMap().GetPlanet().GetRadius();
		Site site;
		site.kind = SiteKind::Settlement;
		site.grid = LocalGrid(radius, PlanetaryCoordinates::ToCartesian({latitude, longitude}));
		site.region = RegionId{};
		site.center = site.grid.Center(site.region);
		site.frame = LocalFrame::At(site.center);
		StartSettlement(site);
	}

	if (view != nullptr && std::string_view(view) == "all" && m_screen == Screen::Local &&
	    m_devFramesInLocalScreen == 0)
	{
		m_viewMode = ViewMode::ShowAll;
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
			LeaveSite(); // back to the planet map
			return;
		}
		UpdateLocal();
		return;
	}

	if (!m_planetViewer)
	{
		return;
	}
	if (IsKeyPressed(KEY_M) && m_activeSite != kNoSite)
	{
		EnterSite(m_activeSite);
		return;
	}
	m_planetViewer->Update();
	if (const std::optional<Site> site = m_planetViewer->TakeFoundedSite())
	{
		StartSettlement(*site);
	}
}

// Centers the camera on the selected object, or on the first worker when
// nothing is selected.
void Application::FocusCamera()
{
	const LocalGame *game = ActiveGame();
	if (game == nullptr)
	{
		return;
	}
	const std::optional<WorldObjectId> selectedId = m_worldInteraction.GetSelectedObject();
	if (selectedId.has_value())
	{
		if (const WorldObject *selected = game->GetWorld().entities.FindObject(*selectedId))
		{
			m_cameraController.FocusOn(selected->position.x, selected->position.z);
			return;
		}
	}
	const WorldPosition worker = game->GetGrid().CellToWorld(game->GetStartingCells().worker);
	m_cameraController.FocusOn(worker.x, worker.z);
}

void Application::UpdateLocal()
{
	if (IsKeyPressed(KEY_F3))
	{
		m_showPathDebug = !m_showPathDebug;
	}
	if (IsKeyPressed(KEY_V))
	{
		m_viewMode = m_viewMode == ViewMode::CutAtLevel ? ViewMode::ShowAll : ViewMode::CutAtLevel;
	}

	LocalGame *active = ActiveGame();
	if (active == nullptr)
	{
		LeaveSite();
		return;
	}
	LocalGame &game = *active;
	AppInternal::UpdateLevelSelection(m_worldRenderer, game.GetGrid(), m_selectedLevel);

	if (IsKeyPressed(KEY_F))
	{
		FocusCamera();
	}
	m_cameraController.Update(m_worldRenderer.GetCamera(), m_time.GetDeltaTime());
	m_chunkViewCenter = GetCameraGridCell(game.GetGrid(), m_worldRenderer.GetCamera().target, m_selectedLevel);
	m_chunkViewCenter.y = m_selectedLevel;
	m_chunkRenderRadius = GetChunkRenderRadius(m_worldRenderer.GetCamera(), game.GetGrid());
	// The ground is drawn as it is, so the chunks above and below the level are
	// needed as far as the eye can usefully tell them apart.
	const int verticalChunkRadius = kVerticalChunkRadius;
	game.GetWorld().terrain.RequestChunksAround(m_chunkViewCenter, m_chunkRenderRadius, verticalChunkRadius);
	game.GetWorld().terrain.UnloadChunksOutside(m_chunkViewCenter, m_chunkRenderRadius + 2, verticalChunkRadius + 1);
	game.GetWorld().terrain.GeneratePendingChunks(kChunksGeneratedPerFrame);
	m_worldInteraction.Update({GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
	                           IsMouseButtonPressed(MOUSE_BUTTON_LEFT), IsKeyPressed(KEY_ESCAPE)},
	                          game.GetGrid(), game.GetWorld(), game.GetWorkSystem(), m_worldRenderer.GetCamera(),
	                          game.GetCommands(), m_config.window.width, m_config.window.height, m_selectedLevel,
	                          m_viewMode);
}
} // namespace Infinitesia
