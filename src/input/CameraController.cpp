#include "input/CameraController.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    namespace
    {
        constexpr float kDegreesToRadians = PI / 180.0f;
        constexpr float kRadiansToDegrees = 180.0f / PI;
        constexpr float kMinimumVectorLength = 1.0e-6f;

        bool IsFinite(Vector3 vector)
        {
            return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
        }

        float GetHorizontalLength(Vector3 vector)
        {
            return std::sqrt(vector.x * vector.x + vector.z * vector.z);
        }
    }

    CameraController::CameraController(CameraControlConfig config)
        : m_config(config)
    {
        if (!std::isfinite(config.movementSpeed) || config.movementSpeed < 0.0f ||
            !std::isfinite(config.rotationSensitivity) || config.rotationSensitivity < 0.0f ||
            !std::isfinite(config.zoomSpeed) || config.zoomSpeed <= 0.0f ||
            !std::isfinite(config.minimumDistance) || config.minimumDistance <= 0.0f ||
            !std::isfinite(config.maximumDistance) || config.maximumDistance < config.minimumDistance ||
            !std::isfinite(config.minimumElevationDegrees) ||
            !std::isfinite(config.maximumElevationDegrees) ||
            config.minimumElevationDegrees <= 0.0f ||
            config.maximumElevationDegrees >= 90.0f ||
            config.maximumElevationDegrees < config.minimumElevationDegrees)
        {
            throw std::invalid_argument("Camera control configuration contains invalid values.");
        }
    }

    void CameraController::Update(Camera3D& camera, float deltaTime) const
    {
        if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
        {
            throw std::invalid_argument("Camera delta time must be finite and non-negative.");
        }

        if (!IsFinite(camera.position) || !IsFinite(camera.target))
        {
            throw std::invalid_argument("Camera position and target must be finite.");
        }

        Vector3 offset{
            camera.position.x - camera.target.x,
            camera.position.y - camera.target.y,
            camera.position.z - camera.target.z
        };
        float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        if (!std::isfinite(distance) || distance <= kMinimumVectorLength)
        {
            throw std::invalid_argument("Camera position must differ from its target.");
        }

        float yaw = std::atan2(offset.x, offset.z);
        float elevation = std::atan2(offset.y, GetHorizontalLength(offset)) * kRadiansToDegrees;

        const bool altPressed = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        if (altPressed)
        {
            const Vector2 mouseDelta = GetMouseDelta();
            yaw -= mouseDelta.x * m_config.rotationSensitivity * kDegreesToRadians;
            elevation = std::clamp(
                elevation + mouseDelta.y * m_config.rotationSensitivity,
                m_config.minimumElevationDegrees,
                m_config.maximumElevationDegrees);
        }

        const float wheelDelta = GetMouseWheelMove();
        if (!std::isfinite(wheelDelta))
        {
            throw std::runtime_error("Mouse wheel returned a non-finite value.");
        }
        distance = std::clamp(
            distance - wheelDelta * m_config.zoomSpeed,
            m_config.minimumDistance,
            m_config.maximumDistance);

        const float movement = m_config.movementSpeed * deltaTime;
        const float forwardX = -std::sin(yaw);
        const float forwardZ = -std::cos(yaw);
        const float rightX = std::cos(yaw);
        const float rightZ = -std::sin(yaw);
        float movementX = 0.0f;
        float movementZ = 0.0f;

        if (IsKeyDown(KEY_W))
        {
            movementX += forwardX;
            movementZ += forwardZ;
        }
        if (IsKeyDown(KEY_S))
        {
            movementX -= forwardX;
            movementZ -= forwardZ;
        }
        if (IsKeyDown(KEY_D))
        {
            movementX += rightX;
            movementZ += rightZ;
        }
        if (IsKeyDown(KEY_A))
        {
            movementX -= rightX;
            movementZ -= rightZ;
        }

        const float movementLength = std::sqrt(movementX * movementX + movementZ * movementZ);
        if (movementLength > kMinimumVectorLength)
        {
            movementX = movementX / movementLength * movement;
            movementZ = movementZ / movementLength * movement;
            camera.target.x += movementX;
            camera.target.z += movementZ;
        }

        const float elevationRadians = elevation * kDegreesToRadians;
        offset = {
            distance * std::sin(yaw) * std::cos(elevationRadians),
            distance * std::sin(elevationRadians),
            distance * std::cos(yaw) * std::cos(elevationRadians)
        };
        camera.position = {
            camera.target.x + offset.x,
            camera.target.y + offset.y,
            camera.target.z + offset.z
        };
    }
}
