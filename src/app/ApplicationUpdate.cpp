#include "app/Application.h"
#include "app/internal/AppHelpers.h"
#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

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
        const int verticalChunkRadius =
            m_selectedLevel % kChunkSize == 0 ||
            m_selectedLevel % kChunkSize == kChunkSize - 1
                ? 1
                : 0;
        m_world.RequestChunksAround(
            m_chunkViewCenter,
            m_chunkRenderRadius,
            verticalChunkRadius);
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
    }
}
