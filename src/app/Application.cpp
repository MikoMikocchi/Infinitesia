#include "app/Application.h"
#include "app/internal/AppHelpers.h"
#include "raylib.h"
#include "commands/ChopTreeCommand.h"
#include "commands/PickUpItemCommand.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Infinitesia
{
    namespace
    {
        constexpr float kDegreesToRadians = PI / 180.0f;
        constexpr std::size_t kChunksGeneratedPerFrame = 4;

        GridCell GetCameraGridCell(const Grid& grid, Vector3 cameraTarget, int selectedLevel)
        {
            const float halfWidth = grid.GetWorldWidth() * 0.5f;
            const float halfDepth = grid.GetWorldDepth() * 0.5f;
            const float edgeInset = grid.GetCellSize() * 0.001f;
            const WorldPosition target{
                std::clamp(cameraTarget.x, -halfWidth + edgeInset, halfWidth - edgeInset),
                static_cast<float>(selectedLevel + 1) * grid.GetCellSize(),
                std::clamp(cameraTarget.z, -halfDepth + edgeInset, halfDepth - edgeInset)
            };
            const std::optional<GridCell> cell = grid.WorldToCell(target);
            if (!cell.has_value())
            {
                throw std::logic_error("Clamped camera target must map to a grid cell.");
            }
            return *cell;
        }

        int GetChunkRenderRadius(const Camera3D& camera, const Grid& grid)
        {
            const float offsetX = camera.position.x - camera.target.x;
            const float offsetY = camera.position.y - camera.target.y;
            const float offsetZ = camera.position.z - camera.target.z;
            const float cameraDistance = std::sqrt(
                offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
            const float aspectRatio = static_cast<float>(GetScreenWidth()) /
                                      static_cast<float>(GetScreenHeight());
            const float visibleHalfWidth =
                cameraDistance * std::tan(camera.fovy * kDegreesToRadians * 0.5f) * aspectRatio;
            const float chunkWorldSize = static_cast<float>(kChunkSize) * grid.GetCellSize();
            return static_cast<int>(std::ceil(visibleHalfWidth / chunkWorldSize)) + 1;
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

        class WorldRendererShaderGuard
        {
        public:
            explicit WorldRendererShaderGuard(WorldRenderer& renderer)
                : m_renderer(renderer)
            {
                m_renderer.LoadShaders();
            }

            ~WorldRendererShaderGuard()
            {
                m_renderer.UnloadShaders();
            }

            WorldRendererShaderGuard(const WorldRendererShaderGuard&) = delete;
            WorldRendererShaderGuard& operator=(const WorldRendererShaderGuard&) = delete;

        private:
            WorldRenderer& m_renderer;
        };
    }

    Application::Application(ApplicationConfig config)
        : m_config(std::move(config)),
          m_grid(m_config.grid),
          m_world(m_grid, m_config.initialWorldObjects, m_config.worldSeed),
          m_simulation(m_config.simulationTicksPerSecond),
          m_workSystem(m_config.worker),
          m_worldRenderer(m_config.camera),
          m_cameraController(m_config.cameraControls),
          m_chunkViewCenter{
              m_config.grid.width / 2,
              m_config.grid.height - 1,
              m_config.grid.depth / 2
          },
          m_selectedLevel(m_config.grid.height - 1),
          m_showPathDebug(m_config.showPathDebug)
    {
        Camera3D& camera = m_worldRenderer.GetCamera();
        camera.position.y += m_grid.GetWorldHeight();
        camera.target.y = m_grid.GetWorldHeight();

        m_commands.Register(std::make_unique<ChopTreeCommand>(m_workSystem, m_grid));
        m_commands.Register(std::make_unique<PickUpItemCommand>());

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
        WorldRendererShaderGuard rendererShaders(m_worldRenderer);
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

        AppInternal::UpdateLevelSelection(m_worldRenderer, m_grid, m_selectedLevel);

        m_cameraController.Update(m_worldRenderer.GetCamera(), m_time.GetDeltaTime());
        m_chunkViewCenter = GetCameraGridCell(
            m_grid,
            m_worldRenderer.GetCamera().target,
            m_selectedLevel);
        m_chunkViewCenter.y = m_selectedLevel;
        m_chunkRenderRadius = GetChunkRenderRadius(m_worldRenderer.GetCamera(), m_grid);
        m_world.RequestChunksAround(m_chunkViewCenter, m_chunkRenderRadius);
        m_world.GeneratePendingChunks(kChunksGeneratedPerFrame);
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
            m_config.window.height,
            m_selectedLevel);

        if (IsKeyPressed(KEY_G))
        {
            const std::optional<WorldObjectId> selectedWorker =
                m_worldInteraction.GetSelectedObject();
            const std::optional<GridCell> targetCell =
                m_worldInteraction.GetHoveredCell();
            if (!selectedWorker.has_value())
            {
                m_worldInteraction.SetActionNotice("Select a worker before digging");
            }
            else if (!targetCell.has_value())
            {
                m_worldInteraction.SetActionNotice("Point at a solid cell to dig");
            }
            else
            {
                switch (m_workSystem.AssignDigOrder(
                    *selectedWorker,
                    *targetCell,
                    m_world,
                    m_grid))
                {
                case WorkOrderResult::Accepted:
                    m_worldInteraction.SetActionNotice("Worker is going to dig");
                    break;
                case WorkOrderResult::InvalidWorker:
                    m_worldInteraction.SetActionNotice("Selected worker is no longer available");
                    break;
                case WorkOrderResult::InvalidTarget:
                    m_worldInteraction.SetActionNotice(
                        "Select a solid cell on the worker's level");
                    break;
                case WorkOrderResult::WorkerBusy:
                    m_worldInteraction.SetActionNotice("Worker already has an order");
                    break;
                case WorkOrderResult::TargetBusy:
                    m_worldInteraction.SetActionNotice("Cell already has a dig order");
                    break;
                case WorkOrderResult::NoReachableWorkCell:
                    m_worldInteraction.SetActionNotice("No reachable cell next to the target");
                    break;
                }
            }
        }
    }    void Application::Render()
    {
        WorldPosition pathOrigin{};
        std::vector<GridCell> debugPath;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (m_showPathDebug && selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = m_world.FindObject(*selectedObjectId);
            if (selectedObject != nullptr &&
                selectedObject->type == WorldObjectType::Worker &&
                selectedObject->cell.y == m_selectedLevel)
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
            debugPath,
            m_chunkViewCenter,
            m_chunkRenderRadius,
            m_selectedLevel);
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
            m_grid.GetWidth(),
            m_grid.GetHeight(),
            m_grid.GetDepth(),
            m_worldInteraction.GetHoveredCell(),
            selectedObjectType,
            workerActivity,
            workProgress,
            m_showPathDebug,
            m_config.window.height,
            m_worldInteraction.GetActionNotice(),
            m_world.GetLoadedChunkCount(),
            m_world.GetTotalChunkCount(),
            m_selectedLevel
        });
    }
}
