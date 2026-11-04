#pragma once

#include "raylib.h"

#include <optional>

namespace Infinitesia
{
    // Every tunable of the RTS-style camera lives here.
    struct CameraControlConfig
    {
        // Panning (WASD / arrows): world units per second per unit of camera distance, so
        // the screen-space speed stays roughly constant at every zoom level.
        float panSpeedPerDistance = 0.9f;
        // Pushing the mouse to the window edge pans too; off unless enabled.
        bool edgePanEnabled = false;
        float edgePanMarginPixels = 8.0f;

        // Zoom (mouse wheel): every notch multiplies the distance by this factor.
        float zoomFactorPerNotch = 1.18f;
        float minimumDistance = 6.0f;
        float maximumDistance = 140.0f;

        // Pitch above the horizon follows the zoom: low when close, near top-down when far.
        float closePitchDegrees = 35.0f;
        float farPitchDegrees = 75.0f;

        // Yaw: Q/E turn in steps; the middle mouse button turns freely.
        float yawStepDegrees = 90.0f;
        float mouseYawDegreesPerPixel = 0.25f;

        // Smoothing rates (1/s): how fast the camera closes the gap to where it is headed.
        // Panning and zooming share one rate so that zooming keeps the cursor's spot in place.
        float moveSmoothing = 12.0f;
        float yawSmoothing = 10.0f;
    };

    class CameraController
    {
    public:
        explicit CameraController(CameraControlConfig config);

        // Puts the camera over (targetX, targetZ) at the given distance and yaw, with no
        // smoothing. The height of the camera target (the shown level) is left to the caller.
        void Reset(Camera3D& camera, float targetX, float targetZ, float distance, float yawRadians = 0.0f);

        // Keeps the camera target within +-halfWidth / +-halfDepth of the world's middle.
        void SetWorldHalfExtents(float halfWidth, float halfDepth);

        // Moves the camera, smoothly, to look at the given spot of the current level.
        void FocusOn(float worldX, float worldZ);

        void Update(Camera3D& camera, float deltaTime);

    private:
        struct State
        {
            float targetX = 0.0f;
            float targetZ = 0.0f;
            float distance = 0.0f;
            float yaw = 0.0f; // radians; the camera sits at (sin, cos) * distance around the target
        };

        float GetPitchRadians(float distance) const;
        Camera3D BuildCamera(const State& state, float targetY, const Camera3D& base) const;
        std::optional<Vector2> GetCursorGroundPoint(const State& state, float targetY, const Camera3D& base) const;
        void ZoomTowardCursor(float notches, float targetY, const Camera3D& base);
        void ClampGoalTarget();

        CameraControlConfig m_config;
        bool m_initialized = false;
        State m_current;
        State m_goal;
        float m_halfWidth;
        float m_halfDepth;
    };
}
