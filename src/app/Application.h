#pragma once

#include <future>
#include <memory>
#include <optional>

#include "app/ApplicationConfig.h"
#include "domains/natural/biology/catalog/SpeciesCatalog.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "core/LoadProgress.h"
#include "core/Simulation.h"
#include "core/Time.h"
#include "gameplay/LocalGame.h"
#include "gameplay/content/GameContent.h"
#include "matter/MatterCatalog.h"
#include "presentation/assets/TextureAssetManager.h"
#include "presentation/input/CameraController.h"
#include "presentation/input/WorldInteractionController.h"
#include "presentation/rendering/WorldRenderer.h"
#include "presentation/ui/DebugOverlay.h"
#include "presentation/ui/PlanetViewer.h"

namespace Infinitesia {
class Application {
 public:
  explicit Application(ApplicationConfig config = {});

  void Run();

 private:
  enum class Screen { Planet, Local };

  void Update();
  void UpdateLocal();
  void FocusCamera();
  void Render();
  void RenderLocal();
  void StartSettlement(const SettlementSite& site);
  void UpdateDevHooks();
  void DrawDebugOverlay();
  void StartPlanetLoad();
  void PollPlanetLoad();
  void DrawLoadingScreen() const;

  ApplicationConfig m_config;
  std::shared_ptr<const GameContent> m_content;
  // The founded settlement; empty until the player founds one.
  std::optional<LocalGame> m_game;
  TextureAssetManager m_textureAssets;
  WorldRenderer m_worldRenderer;
  CameraController m_cameraController;
  WorldInteractionController m_worldInteraction;
  DebugOverlay m_debugOverlay;
  // Built on a worker thread after the window opens; null while loading.
  std::unique_ptr<PlanetViewer> m_planetViewer;
  // Read once when the planet starts loading; the settlement reuses them.
  std::shared_ptr<const MatterCatalog> m_matter;
  std::shared_ptr<const SpeciesCatalog> m_species;
  // Declared before m_planetLoad: the future's destructor joins the worker,
  // which still writes to this, so it must be destroyed after the future.
  LoadProgress m_loadProgress;
  std::future<std::unique_ptr<PlanetViewer>> m_planetLoad;
  Screen m_screen = Screen::Planet;
  bool m_exitRequested = false;
  int m_devFramesInLocalScreen = 0;
  GridCell m_chunkViewCenter{};
  int m_chunkRenderRadius = 1;
  int m_selectedLevel = 0;
  ViewMode m_viewMode = ViewMode::CutAtLevel;
  bool m_showPathDebug = false;
  Time m_time;
};
}  // namespace Infinitesia
