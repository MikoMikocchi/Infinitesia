#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "gameplay/content/ContentTypes.h"
#include "gameplay/work/WorkSystem.h"
#include "world/Grid.h"

namespace Infinitesia {
struct DebugOverlayData {
  int framesPerSecond;
  double totalTime;
  std::uint64_t tickCount;
  int gridWidth;
  int gridHeight;
  int gridDepth;
  std::optional<GridCell> hoveredCell;
  const ObjectKindDef* selectedObjectKind;  // null when nothing is selected
  std::optional<WorkerActivity> workerActivity;
  float workProgress;
  bool pathDebugEnabled;
  int screenHeight;
  const std::string& actionNotice;
  std::size_t loadedChunkCount;
  std::size_t totalChunkCount;
  int selectedLevel;
  bool showAllLevels;
};

class DebugOverlay {
 public:
  void Draw(const DebugOverlayData& data) const;
};
}  // namespace Infinitesia
