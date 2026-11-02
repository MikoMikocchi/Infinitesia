#include "app/Application.h"
#include "raylib.h"

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        const char* GetWorkOrderMessage(WorkOrderResult result)
        {
            switch (result)
            {
            case WorkOrderResult::Accepted:
                return "Worker is moving to the tree";
            case WorkOrderResult::InvalidWorker:
                return "Selected worker is no longer available";
            case WorkOrderResult::InvalidTarget:
                return "Tree is no longer available";
            case WorkOrderResult::WorkerBusy:
                return "Worker already has an order";
            case WorkOrderResult::TargetBusy:
                return "Tree is already being chopped";
            case WorkOrderResult::NoReachableWorkCell:
                return "No free adjacent cell for the worker";
            }

            return "Could not assign work order";
        }

        class WindowGuard
        {
        public:
            explicit WindowGuard(const WindowConfig& config)
            {
                if (config.width <= 0 || config.height <= 0 || config.title.empty() ||
                    config.targetFramesPerSecond < 0)
                {
                    throw std::invalid_argument("Window configuration contains invalid values.");
                }

                InitWindow(config.width, config.height, config.title.c_str());
                if (!IsWindowReady())
                {
                    CloseWindow();
                    throw std::runtime_error("Failed to initialize the application window.");
                }
            }

            ~WindowGuard()
            {
                CloseWindow();
            }

            WindowGuard(const WindowGuard&) = delete;
            WindowGuard& operator=(const WindowGuard&) = delete;
        };
    }

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_grid(m_config.grid),
          m_world(m_grid, m_config.initialWorldObjects),
          m_simulation(m_config.simulationTicksPerSecond),
          m_workSystem(m_config.worker),
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

    void Application::Run()
    {
        WindowGuard window(m_config.window);
        SetTargetFPS(m_config.window.targetFramesPerSecond);

        while (!WindowShouldClose())
        {
            m_time.Advance(GetFrameTime());
            const std::uint64_t advancedTicks = m_simulation.AdvanceFrame(m_time.GetDeltaTime());
            for (std::uint64_t tick = 0; tick < advancedTicks; ++tick)
            {
                m_workSystem.Tick(m_world, m_grid, m_simulation.GetFixedDeltaTime());
            }
            Update();
            Render();
        }
    }

    void Application::Update()
    {
        if (IsKeyPressed(KEY_F3))
        {
            m_showPathDebug = !m_showPathDebug;
        }

        m_cameraController.Update(m_worldRenderer.GetCamera(), m_time.GetDeltaTime());
        const Vector2 mousePosition = GetMousePosition();
        m_hoveredCell = m_gridPointer.PickCell(
            m_grid,
            m_worldRenderer.GetCamera(),
            mousePosition);
        if (m_contextMenu.BlocksWorldInput(mousePosition))
        {
            m_hoveredObject.reset();
        }
        else
        {
            m_hoveredObject = m_worldObjectPicker.PickObject(
                m_grid,
                m_world,
                m_worldRenderer.GetCamera(),
                mousePosition);
        }

        std::optional<WorldObjectId> hoveredTree;
        if (m_hoveredObject.has_value())
        {
            const WorldObject* hoveredObject = m_world.FindObject(*m_hoveredObject);
            if (hoveredObject != nullptr && hoveredObject->type == WorldObjectType::Tree)
            {
                hoveredTree = hoveredObject->id;
            }
        }

        std::optional<WorldObjectId> selectedWorker;
        if (m_selectedObject.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*m_selectedObject);
            if (selectedObject != nullptr && selectedObject->type == WorldObjectType::Worker)
            {
                selectedWorker = selectedObject->id;
            }
        }

        const ContextMenuUpdate menuUpdate = m_contextMenu.Update(
            mousePosition,
            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            IsKeyPressed(KEY_ESCAPE),
            hoveredTree,
            selectedWorker,
            m_config.window.width,
            m_config.window.height);

        if (menuUpdate.actionRequest.has_value())
        {
            const WorldActionRequest& request = *menuUpdate.actionRequest;
            if (request.action == WorldAction::Chop)
            {
                m_actionNotice = GetWorkOrderMessage(
                    m_workSystem.AssignChopOrder(request.actor, request.target, m_world, m_grid));
            }
        }
        else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !menuUpdate.consumedMouseClick)
        {
            m_selectedObject = m_hoveredObject;
            m_actionNotice.clear();
        }
    }

    void Application::Render()
    {
        WorldPosition pathOrigin{};
        std::vector<GridCell> debugPath;
        if (m_showPathDebug && m_selectedObject.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*m_selectedObject);
            if (selectedObject != nullptr && selectedObject->type == WorldObjectType::Worker)
            {
                pathOrigin = selectedObject->position;
                debugPath = m_workSystem.GetRemainingPath(selectedObject->id);
            }
        }

        BeginDrawing();
        ClearBackground({ 30, 34, 39, 255 });

        m_worldRenderer.Draw(
            m_grid,
            m_world,
            m_hoveredCell,
            m_hoveredObject,
            m_selectedObject,
            pathOrigin,
            debugPath);
        DrawText(
            m_config.window.title.c_str(),
            (m_config.window.width - MeasureText(m_config.window.title.c_str(), 24)) / 2,
            12,
            24,
            RAYWHITE);
        DrawDebugOverlay();
        m_contextMenu.Draw();

        EndDrawing();
    }

    void Application::DrawDebugOverlay()
    {
        const int padding = 8;
        const int fontSize = 16;
        const int lineHeight = 20;
        const int panelWidth = 250;
        const int panelHeight = 8 * lineHeight + padding;

        char buf[128];

        DrawRectangle(padding - 4, padding - 4, panelWidth, panelHeight, Fade(BLACK, 0.4f));

        std::snprintf(buf, sizeof(buf), "FPS: %d", GetFPS());
        DrawText(buf, padding, padding, fontSize, LIME);

        std::snprintf(buf, sizeof(buf), "Time since start: %.3f s", m_time.GetTotalTime());
        DrawText(buf, padding, padding + lineHeight, fontSize, SKYBLUE);

        std::snprintf(buf, sizeof(buf), "Ticks: %llu", static_cast<unsigned long long>(m_simulation.GetTickCount()));
        DrawText(buf, padding, padding + 2 * lineHeight, fontSize, ORANGE);

        std::snprintf(buf, sizeof(buf), "Grid: %d x %d", m_grid.GetColumns(), m_grid.GetRows());
        DrawText(buf, padding, padding + 3 * lineHeight, fontSize, RAYWHITE);

        if (m_hoveredCell.has_value())
        {
            std::snprintf(
                buf,
                sizeof(buf),
                "Cell: %d, %d",
                m_hoveredCell->column,
                m_hoveredCell->row);
            DrawText(buf, padding, padding + 4 * lineHeight, fontSize, GOLD);
        }

        if (m_selectedObject.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*m_selectedObject);
            if (selectedObject != nullptr)
            {
                const char* typeName = "Logs";
                if (selectedObject->type == WorldObjectType::Worker)
                {
                    typeName = "Worker";
                }
                else if (selectedObject->type == WorldObjectType::Tree)
                {
                    typeName = "Tree";
                }
                std::snprintf(buf, sizeof(buf), "Selected: %s", typeName);
                DrawText(buf, padding, padding + 5 * lineHeight, fontSize, GREEN);

                if (selectedObject->type == WorldObjectType::Worker)
                {
                    const WorkerActivity activity = m_workSystem.GetActivity(selectedObject->id);
                    const char* activityName = "Idle";
                    if (activity == WorkerActivity::Moving)
                    {
                        activityName = "Moving";
                    }
                    else if (activity == WorkerActivity::Working)
                    {
                        std::snprintf(
                            buf,
                            sizeof(buf),
                            "Chopping: %.0f%%",
                            m_workSystem.GetWorkProgress(selectedObject->id) * 100.0f);
                        DrawText(buf, padding, padding + 6 * lineHeight, fontSize, ORANGE);
                        activityName = nullptr;
                    }

                    if (activityName != nullptr)
                    {
                        std::snprintf(buf, sizeof(buf), "Activity: %s", activityName);
                        DrawText(buf, padding, padding + 6 * lineHeight, fontSize, ORANGE);
                    }
                }
            }
        }

        if (!m_actionNotice.empty())
        {
            DrawText(
                m_actionNotice.c_str(),
                padding,
                m_config.window.height - lineHeight - padding,
                fontSize,
                GOLD);
        }

        std::snprintf(buf, sizeof(buf), "Path debug: %s (F3)", m_showPathDebug ? "on" : "off");
        DrawText(buf, padding, padding + 7 * lineHeight, fontSize, SKYBLUE);
    }
}
