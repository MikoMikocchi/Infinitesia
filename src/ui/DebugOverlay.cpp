#include "ui/DebugOverlay.h"

#include "raylib.h"

#include <cstdio>
#include <cstddef>

namespace Infinitesia
{
    void DebugOverlay::Draw(const DebugOverlayData& data) const
    {
        constexpr int padding = 8;
        constexpr int fontSize = 16;
        constexpr int lineHeight = 20;
        constexpr int panelWidth = 250;
        constexpr int panelHeight = 10 * lineHeight + padding;

        char buffer[128];

        DrawRectangle(padding - 4, padding - 4, panelWidth, panelHeight, Fade(BLACK, 0.4f));

        std::snprintf(buffer, sizeof(buffer), "FPS: %d", data.framesPerSecond);
        DrawText(buffer, padding, padding, fontSize, LIME);

        std::snprintf(buffer, sizeof(buffer), "Time since start: %.3f s", data.totalTime);
        DrawText(buffer, padding, padding + lineHeight, fontSize, SKYBLUE);

        std::snprintf(
            buffer,
            sizeof(buffer),
            "Ticks: %llu",
            static_cast<unsigned long long>(data.tickCount));
        DrawText(buffer, padding, padding + 2 * lineHeight, fontSize, ORANGE);

        std::snprintf(
            buffer,
            sizeof(buffer),
            "Grid: %d x %d x %d",
            data.gridWidth,
            data.gridHeight,
            data.gridDepth);
        DrawText(buffer, padding, padding + 3 * lineHeight, fontSize, RAYWHITE);

        std::snprintf(
            buffer,
            sizeof(buffer),
            "Chunks: %zu / %zu",
            data.loadedChunkCount,
            data.totalChunkCount);
        DrawText(buffer, padding, padding + 4 * lineHeight, fontSize, RAYWHITE);

        std::snprintf(
            buffer,
            sizeof(buffer),
            "Level: %d (Page Up/Down)",
            data.selectedLevel);
        DrawText(buffer, padding, padding + 5 * lineHeight, fontSize, GOLD);

        if (data.hoveredCell.has_value())
        {
            std::snprintf(
                buffer,
                sizeof(buffer),
                "Cell: %d, %d, %d",
                data.hoveredCell->x,
                data.hoveredCell->y,
                data.hoveredCell->z);
            DrawText(buffer, padding, padding + 6 * lineHeight, fontSize, GOLD);
        }

        if (data.selectedObjectType.has_value())
        {
            const char* typeName = "Item";
            if (*data.selectedObjectType == WorldObjectType::Worker)
            {
                typeName = "Worker";
            }
            else if (*data.selectedObjectType == WorldObjectType::Tree)
            {
                typeName = "Tree";
            }
            std::snprintf(buffer, sizeof(buffer), "Selected: %s", typeName);
            DrawText(buffer, padding, padding + 7 * lineHeight, fontSize, GREEN);

            if (data.workerActivity.has_value())
            {
                const char* activityName = "Idle";
                if (*data.workerActivity == WorkerActivity::Moving)
                {
                    activityName = "Moving";
                }
                else if (*data.workerActivity == WorkerActivity::Working)
                {
                    std::snprintf(
                        buffer,
                        sizeof(buffer),
                        "Chopping: %.0f%%",
                        data.workProgress * 100.0f);
                    DrawText(buffer, padding, padding + 8 * lineHeight, fontSize, ORANGE);
                    activityName = nullptr;
                }

                if (activityName != nullptr)
                {
                    std::snprintf(buffer, sizeof(buffer), "Activity: %s", activityName);
                    DrawText(buffer, padding, padding + 8 * lineHeight, fontSize, ORANGE);
                }
            }
        }

        if (!data.actionNotice.empty())
        {
            DrawText(
                data.actionNotice.c_str(),
                padding,
                data.screenHeight - lineHeight - padding,
                fontSize,
                GOLD);
        }

        std::snprintf(
            buffer,
            sizeof(buffer),
            "Path debug: %s (F3)",
            data.pathDebugEnabled ? "on" : "off");
        DrawText(buffer, padding, padding + 9 * lineHeight, fontSize, SKYBLUE);
    }
}
