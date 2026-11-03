#include "app/Application.h"
#include "raylib.h"

namespace Infinitesia
{
    void Application::Render()
    {
        BeginDrawing();
        if (m_planetViewer)
        {
            m_planetViewer->Draw();
        }
        else
        {
            DrawLoadingScreen();
        }
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
