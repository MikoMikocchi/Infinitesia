#pragma once

#include <cstdint>
#include <string>

#include "gameplay/work/WorkSystem.h"
#include "presentation/input/CameraController.h"
#include "presentation/rendering/WorldRenderer.h"
#include "world/Grid.h"

namespace Infinitesia
{
struct WindowConfig
{
	int width;
	int height;
	int targetFramesPerSecond;
	std::string title;
};

struct ApplicationConfig
{
	WindowConfig window{1280, 720, 60, "Infinitesia"};
	// The world of the founded settlement: a 1 km square window of the planet,
	// 256 m tall, with the ground of its middle column at level 112. Chunks are
	// streamed around the camera; untouched chunks are unloaded when they leave
	// the view.
	GridConfig settlementGrid{1024, 256, 1024, 1.0f};
	int settlementAnchorLevel = 112;
	// The starting distance and pitch; the pitch then follows the zoom (see
	// CameraControlConfig).
	CameraConfig camera{30.0f, 45.0f, 40.0f};
	CameraControlConfig cameraControls{};
	WorkerConfig worker{3.0f, 4.0f, 3.0f, 7000.0f};
	bool showPathDebug = false;
	float simulationTicksPerSecond = 20.0f;
	std::uint32_t worldSeed = 1234567890;
};
} // namespace Infinitesia
