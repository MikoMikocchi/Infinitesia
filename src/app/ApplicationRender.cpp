#include "app/Application.h"
#include "raylib.h"

namespace Infinitesia
{
    void Application::Render()
    {
        BeginDrawing();
        if (m_screen == Screen::Local)
        {
            RenderLocal();
        }
        else if (m_planetViewer)
        {
            m_planetViewer->Draw();
        }
        else
        {
            DrawLoadingScreen();
        }
        EndDrawing();
    }

    void Application::RenderLocal()
    {
        WorldPosition pathOrigin{};
        std::vector<GridCell> debugPath;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (m_showPathDebug && selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = m_world.entities.FindObject(*selectedObjectId);
            if (selectedObject != nullptr &&
                selectedObject->type == WorldObjectType::Worker &&
                selectedObject->cell.y == m_selectedLevel)
            {
                pathOrigin = selectedObject->position;
                debugPath = m_workSystem.GetRemainingPath(selectedObject->id);
            }
        }

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
        DrawText("M: planet map", m_config.window.width - 150, 56, 16, { 139, 158, 177, 255 });
        DrawDebugOverlay();
        m_worldInteraction.DrawOverlay(m_world.entities, m_textureAssets);
    }

    void Application::DrawDebugOverlay()
    {
        std::optional<WorldObjectType> selectedObjectType;
        std::optional<WorkerActivity> workerActivity;
        float workProgress = 0.0f;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = m_world.entities.FindObject(*selectedObjectId);
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
            m_world.terrain.GetLoadedChunkCount(),
            m_world.terrain.GetTotalChunkCount(),
            m_selectedLevel
        });
    }
}
