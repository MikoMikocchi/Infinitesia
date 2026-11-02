#pragma once

#include "app/ApplicationConfig.h"
#include "assets/TextureAssetManager.h"
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
        PlanetViewer m_planetViewer;
        GridCell m_chunkViewCenter{};
        int m_chunkRenderRadius = 1;
        int m_selectedLevel = 0;
        bool m_showPathDebug = true;
        Time m_time;
    };
}
