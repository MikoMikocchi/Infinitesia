#pragma once

#include "domains/natural/planetary/PlanetMap.h"
#include "raylib.h"

// Where the parts of the planet viewer are on the screen.
namespace Infinitesia
{
    inline constexpr float kPanelPadding = 24.0f;
    inline constexpr float kHeaderHeight = 82.0f;
    inline constexpr float kFooterHeight = 92.0f;
    inline constexpr float kCameraFieldOfView = 45.0f;
    inline constexpr float kPi = 3.14159265358979323846f;

    struct ViewerLayout
    {
        Rectangle mapPanel{};
        Rectangle globePanel{};
        Rectangle mapBounds{};
        Rectangle regionBounds{};
        Vector2 globeCenter{};
        float globeRadius = 0.0f;
    };

    ViewerLayout GetLayout(int screenWidth, int screenHeight, const PlanetMap& planetMap);
    float GetMinimumZoom(const ViewerLayout& layout, int screenHeight);
    bool RectangleContains(Rectangle rectangle, Vector2 point);
}
