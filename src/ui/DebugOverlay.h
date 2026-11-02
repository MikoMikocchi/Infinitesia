#pragma once

#include "world/Grid.h"
#include "world/WorkSystem.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Infinitesia
{
    struct DebugOverlayData
    {
        int framesPerSecond;
        double totalTime;
        std::uint64_t tickCount;
        int gridWidth;
        int gridHeight;
        int gridDepth;
        std::optional<GridCell> hoveredCell;
        std::optional<WorldObjectType> selectedObjectType;
        std::optional<WorkerActivity> workerActivity;
        float workProgress;
        bool pathDebugEnabled;
        int screenHeight;
        const std::string& actionNotice;
        std::size_t loadedChunkCount;
        std::size_t totalChunkCount;
        int selectedLevel;
    };

    class DebugOverlay
    {
    public:
        void Draw(const DebugOverlayData& data) const;
    };
}
