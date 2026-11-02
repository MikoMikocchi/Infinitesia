#pragma once

#include "app/ApplicationConfig.h"
#include "core/Simulation.h"
#include "core/Time.h"
#include "input/CameraController.h"
#include "input/GridPointer.h"
#include "input/WorldObjectPicker.h"
#include "rendering/WorldRenderer.h"
#include "ui/WorldContextMenu.h"
#include "world/Grid.h"
#include "world/WorkSystem.h"

#include <optional>
#include <string>

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
        WorldRenderer m_worldRenderer;
        CameraController m_cameraController;
        GridPointer m_gridPointer;
        WorldObjectPicker m_worldObjectPicker;
        WorldContextMenu m_contextMenu;
        std::optional<GridCell> m_hoveredCell;
        std::optional<WorldObjectId> m_hoveredObject;
        std::optional<WorldObjectId> m_selectedObject;
        std::string m_actionNotice;
        Time m_time;
    };
}
