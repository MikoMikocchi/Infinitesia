#include "app/Application.h"
#include "raylib.h"
#include "world/ChopTreeCommand.h"
#include "world/PickUpLogsCommand.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Infinitesia
{
    namespace
    {
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
        m_commands.Register(std::make_unique<ChopTreeCommand>(m_workSystem, m_grid));
        m_commands.Register(std::make_unique<PickUpLogsCommand>());

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
        m_worldInteraction.Update(
            {
                GetMousePosition(),
                IsMouseButtonPressed(MOUSE_BUTTON_RIGHT),
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                IsKeyPressed(KEY_ESCAPE)
            },
            m_grid,
            m_world,
            m_worldRenderer.GetCamera(),
            m_commands,
            m_config.window.width,
            m_config.window.height);
    }

    void Application::Render()
    {
        WorldPosition pathOrigin{};
        std::vector<GridCell> debugPath;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (m_showPathDebug && selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*selectedObjectId);
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
            m_worldInteraction.GetHoveredCell(),
            m_worldInteraction.GetHoveredObject(),
            selectedObjectId,
            pathOrigin,
            debugPath);
        DrawText(
            m_config.window.title.c_str(),
            (m_config.window.width - MeasureText(m_config.window.title.c_str(), 24)) / 2,
            12,
            24,
            RAYWHITE);
        DrawDebugOverlay();
        m_worldInteraction.DrawOverlay();

        EndDrawing();
    }

    void Application::DrawDebugOverlay()
    {
        std::optional<WorldObjectType> selectedObjectType;
        std::optional<WorkerActivity> workerActivity;
        float workProgress = 0.0f;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*selectedObjectId);
            if (selectedObject != nullptr)
            {
                selectedObjectType = selectedObject->type;
                if (selectedObject->type == WorldObjectType::Worker)
                {
                    const WorkerActivity activity = m_workSystem.GetActivity(selectedObject->id);
                    workerActivity = activity;
                    if (activity == WorkerActivity::Working)
                    {
                        workProgress = m_workSystem.GetWorkProgress(selectedObject->id);
                    }
                }
            }
        }

        m_debugOverlay.Draw({
            GetFPS(),
            m_time.GetTotalTime(),
            m_simulation.GetTickCount(),
            m_grid.GetColumns(),
            m_grid.GetRows(),
            m_worldInteraction.GetHoveredCell(),
            selectedObjectType,
            workerActivity,
            workProgress,
            m_showPathDebug,
            m_config.window.height,
            m_worldInteraction.GetActionNotice()
        });
    }
}
