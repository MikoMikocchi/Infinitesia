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
        LocalGame& game = *m_game;
        WorldPosition pathOrigin{};
        std::vector<GridCell> debugPath;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (m_showPathDebug && selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = game.GetWorld().entities.FindObject(*selectedObjectId);
            if (selectedObject != nullptr &&
                selectedObject->Is(ObjectAbility::Worker) &&
                selectedObject->cell.y == m_selectedLevel)
            {
                pathOrigin = selectedObject->position;
                debugPath = game.GetWorkSystem().GetRemainingPath(selectedObject->id);
            }
        }

        ClearBackground({ 30, 34, 39, 255 });
        m_worldRenderer.Draw(
            game.GetGrid(),
            game.GetWorld(),
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
        m_worldInteraction.DrawOverlay(game.GetWorld().entities, m_textureAssets);
    }

    void Application::DrawDebugOverlay()
    {
        const LocalGame& game = *m_game;
        const ObjectKindDef* selectedObjectKind = nullptr;
        std::optional<WorkerActivity> workerActivity;
        float workProgress = 0.0f;
        const std::optional<WorldObjectId> selectedObjectId = m_worldInteraction.GetSelectedObject();
        if (selectedObjectId.has_value())
        {
            const WorldObject* selectedObject = game.GetWorld().entities.FindObject(*selectedObjectId);
            if (selectedObject != nullptr)
            {
                selectedObjectKind = selectedObject->kind;
                if (selectedObject->Is(ObjectAbility::Worker))
                {
                    const WorkerActivity activity = game.GetWorkSystem().GetActivity(selectedObject->id);
                    workerActivity = activity;
                    if (activity == WorkerActivity::Working)
                    {
                        workProgress = game.GetWorkSystem().GetWorkProgress(selectedObject->id);
                    }
                }
            }
        }

        m_debugOverlay.Draw({
            GetFPS(),
            m_time.GetTotalTime(),
            game.GetSimulation().GetTickCount(),
            game.GetGrid().GetWidth(),
            game.GetGrid().GetHeight(),
            game.GetGrid().GetDepth(),
            m_worldInteraction.GetHoveredCell(),
            selectedObjectKind,
            workerActivity,
            workProgress,
            m_showPathDebug,
            m_config.window.height,
            m_worldInteraction.GetActionNotice(),
            game.GetWorld().terrain.GetLoadedChunkCount(),
            game.GetWorld().terrain.GetTotalChunkCount(),
            m_selectedLevel
        });
    }
}
