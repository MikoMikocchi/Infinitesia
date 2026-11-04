#pragma once

#include <cstdint>
#include <string>

#include "gameplay/work/WorkSystem.h"
#include "presentation/input/CameraController.h"
#include "presentation/rendering/WorldRenderer.h"
#include "world/Grid.h"

namespace Infinitesia {
struct WindowConfig {
  int width;
  int height;
  int targetFramesPerSecond;
  std::string title;
};

struct ApplicationConfig {
  WindowConfig window{1920, 1080, 60, "Infinitesia"};
  // The world of the founded settlement: a 256 m square window of the planet,
  // 192 m tall, with the ground of its middle column at level 112.
  GridConfig settlementGrid{256, 192, 256, 1.0f};
  int settlementAnchorLevel = 112;
  // The starting distance and pitch; the pitch then follows the zoom (see
  // CameraControlConfig).
  CameraConfig camera{30.0f, 45.0f, 40.0f};
  CameraControlConfig cameraControls{};
  WorkerConfig worker{3.0f, 4.0f, 3.0f, 7000.0f};
  bool showPathDebug = false;
  float simulationTicksPerSecond = 20.0f;
  std::uint32_t worldSeed = 1;
};
}  // namespace Infinitesia
