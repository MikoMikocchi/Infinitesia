#pragma once

#include "app/ApplicationConfig.h"
#include "gameplay/content/GameContent.h"
#include "presentation/assets/TextureAssetManager.h"
#include "foundation/LoadProgress.h"
#include "foundation/Simulation.h"
#include "foundation/Time.h"
#include "presentation/input/CameraController.h"
#include "presentation/input/WorldInteractionController.h"
#include "presentation/rendering/WorldRenderer.h"
#include "presentation/ui/DebugOverlay.h"
#include "presentation/ui/PlanetViewer.h"
#include "gameplay/LocalGame.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"

#include <future>
#include <memory>
#include <optional>

namespace Infinitesia
{
    class Application
    {
    public:
        explicit Application(ApplicationConfig config = {});

        void Run();

    private:
        enum class Screen
        {
            Planet,
            Local
        };

        void Update();
        void UpdateLocal();
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
        bool m_showPathDebug = true;
        Time m_time;
    };
}
