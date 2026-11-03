#pragma once

#include "app/ApplicationConfig.h"
#include "assets/TextureAssetManager.h"
#include "core/LoadProgress.h"
#include "core/Simulation.h"
#include "core/Time.h"
#include "input/CameraController.h"
#include "input/WorldInteractionController.h"
#include "rendering/WorldRenderer.h"
#include "ui/DebugOverlay.h"
#include "ui/PlanetViewer.h"
#include "commands/CommandRegistry.h"
#include "world/Grid.h"
#include "world/WorkSystem.h"

#include <future>
#include <memory>

namespace Infinitesia
{
    class Application
    {
    public:
        explicit Application(ApplicationConfig config = {});

        void Run();

    private:
        void Update();
        void Render();
        void DrawDebugOverlay();
        void StartPlanetLoad();
        void PollPlanetLoad();
        void DrawLoadingScreen() const;

        ApplicationConfig m_config;
        Grid m_grid;
        World m_world;
        Simulation m_simulation;
        WorkSystem m_workSystem;
        CommandRegistry m_commands;
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
        GridCell m_chunkViewCenter{};
        int m_chunkRenderRadius = 1;
        int m_selectedLevel = 0;
        bool m_showPathDebug = true;
        Time m_time;
    };
}
