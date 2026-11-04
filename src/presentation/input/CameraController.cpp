#include "presentation/input/CameraController.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Infinitesia {
namespace {
constexpr float kDegreesToRadians = PI / 180.0f;
constexpr float kMinimumVectorLength = 1.0e-6f;
// A ray this close to horizontal never reaches the level plane in a useful
// place.
constexpr float kMinimumDownwardRay = 1.0e-4f;

bool IsFinite(Vector3 vector) {
  return std::isfinite(vector.x) && std::isfinite(vector.y) &&
         std::isfinite(vector.z);
}

bool IsPositiveFinite(float value) {
  return std::isfinite(value) && value > 0.0f;
}

// Share of the gap closed in one frame by an exponential approach at the given
// rate.
float GetSmoothingBlend(float rate, float deltaTime) {
  return 1.0f - std::exp(-rate * deltaTime);
}
}  // namespace

CameraController::CameraController(CameraControlConfig config)
    : m_config(config),
      m_halfWidth(std::numeric_limits<float>::max()),
      m_halfDepth(std::numeric_limits<float>::max()) {
  if (!IsPositiveFinite(config.panSpeedPerDistance) ||
      !std::isfinite(config.edgePanMarginPixels) ||
      config.edgePanMarginPixels < 0.0f ||
      !std::isfinite(config.zoomFactorPerNotch) ||
      config.zoomFactorPerNotch <= 1.0f ||
      !IsPositiveFinite(config.minimumDistance) ||
      !std::isfinite(config.maximumDistance) ||
      config.maximumDistance < config.minimumDistance ||
      !std::isfinite(config.closePitchDegrees) ||
      !std::isfinite(config.farPitchDegrees) ||
      config.closePitchDegrees <= 0.0f || config.farPitchDegrees >= 90.0f ||
      config.farPitchDegrees < config.closePitchDegrees ||
      !std::isfinite(config.yawStepDegrees) ||
      !std::isfinite(config.mouseYawDegreesPerPixel) ||
      config.mouseYawDegreesPerPixel < 0.0f ||
      !IsPositiveFinite(config.moveSmoothing) ||
      !IsPositiveFinite(config.yawSmoothing)) {
    throw std::invalid_argument(
        "Camera control configuration contains invalid values.");
  }
}

void CameraController::Reset(Camera3D& camera, float targetX, float targetZ,
                             float distance, float yawRadians) {
  m_goal.targetX = targetX;
  m_goal.targetZ = targetZ;
  m_goal.distance =
      std::clamp(distance, m_config.minimumDistance, m_config.maximumDistance);
  m_goal.yaw = yawRadians;
  ClampGoalTarget();
  m_current = m_goal;
  m_initialized = true;
  camera = BuildCamera(m_current, camera.target.y, camera);
}

void CameraController::SetWorldHalfExtents(float halfWidth, float halfDepth) {
  m_halfWidth = halfWidth;
  m_halfDepth = halfDepth;
  ClampGoalTarget();
}

void CameraController::FocusOn(float worldX, float worldZ) {
  m_goal.targetX = worldX;
  m_goal.targetZ = worldZ;
  ClampGoalTarget();
}

float CameraController::GetPitchRadians(float distance) const {
  const float range = m_config.maximumDistance - m_config.minimumDistance;
  const float zoom =
      range > kMinimumVectorLength
          ? std::clamp((distance - m_config.minimumDistance) / range, 0.0f,
                       1.0f)
          : 0.0f;
  const float eased = zoom * zoom * (3.0f - 2.0f * zoom);  // smoothstep
  const float pitch =
      m_config.closePitchDegrees +
      (m_config.farPitchDegrees - m_config.closePitchDegrees) * eased;
  return pitch * kDegreesToRadians;
}

Camera3D CameraController::BuildCamera(const State& state, float targetY,
                                       const Camera3D& base) const {
  const float pitch = GetPitchRadians(state.distance);
  const float horizontal = state.distance * std::cos(pitch);
  Camera3D camera = base;
  camera.target = {state.targetX, targetY, state.targetZ};
  camera.position = {state.targetX + horizontal * std::sin(state.yaw),
                     targetY + state.distance * std::sin(pitch),
                     state.targetZ + horizontal * std::cos(state.yaw)};
  return camera;
}

std::optional<Vector2> CameraController::GetCursorGroundPoint(
    const State& state, float targetY, const Camera3D& base) const {
  const Ray ray =
      GetMouseRay(GetMousePosition(), BuildCamera(state, targetY, base));
  if (ray.direction.y > -kMinimumDownwardRay) {
    return std::nullopt;
  }
  const float along = (targetY - ray.position.y) / ray.direction.y;
  if (along <= 0.0f) {
    return std::nullopt;
  }
  return Vector2{ray.position.x + ray.direction.x * along,
                 ray.position.z + ray.direction.z * along};
}

// Changes the goal distance, then slides the goal target so that the spot of
// the level plane under the cursor is the same before and after (pitch changes
// with distance, so the shift is measured with the zoomed camera instead of
// being derived from a ratio).
void CameraController::ZoomTowardCursor(float notches, float targetY,
                                        const Camera3D& base) {
  const float distance = std::clamp(
      m_goal.distance * std::pow(m_config.zoomFactorPerNotch, -notches),
      m_config.minimumDistance, m_config.maximumDistance);
  if (distance == m_goal.distance) {
    return;
  }

  const std::optional<Vector2> before =
      GetCursorGroundPoint(m_goal, targetY, base);
  m_goal.distance = distance;
  if (before.has_value()) {
    if (const std::optional<Vector2> after =
            GetCursorGroundPoint(m_goal, targetY, base)) {
      m_goal.targetX += before->x - after->x;
      m_goal.targetZ += before->y - after->y;
    }
  }
  ClampGoalTarget();
}

void CameraController::ClampGoalTarget() {
  m_goal.targetX = std::clamp(m_goal.targetX, -m_halfWidth, m_halfWidth);
  m_goal.targetZ = std::clamp(m_goal.targetZ, -m_halfDepth, m_halfDepth);
}

void CameraController::Update(Camera3D& camera, float deltaTime) {
  if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
    throw std::invalid_argument(
        "Camera delta time must be finite and non-negative.");
  }
  if (!IsFinite(camera.position) || !IsFinite(camera.target)) {
    throw std::invalid_argument("Camera position and target must be finite.");
  }

  if (!m_initialized) {
    const Vector3 offset{camera.position.x - camera.target.x,
                         camera.position.y - camera.target.y,
                         camera.position.z - camera.target.z};
    const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y +
                                     offset.z * offset.z);
    if (!std::isfinite(distance) || distance <= kMinimumVectorLength) {
      throw std::invalid_argument(
          "Camera position must differ from its target.");
    }
    Reset(camera, camera.target.x, camera.target.z, distance,
          std::atan2(offset.x, offset.z));
  }

  // The shown level moves the camera height from outside; it is the zoom plane.
  const float targetY = camera.target.y;

  // Yaw: Q/E turn a step, the middle mouse button turns freely (and at once).
  const float yawStep = m_config.yawStepDegrees * kDegreesToRadians;
  if (IsKeyPressed(KEY_Q)) {
    m_goal.yaw += yawStep;
  }
  if (IsKeyPressed(KEY_E)) {
    m_goal.yaw -= yawStep;
  }
  if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    const float turn = GetMouseDelta().x * m_config.mouseYawDegreesPerPixel *
                       kDegreesToRadians;
    m_goal.yaw -= turn;
    m_current.yaw -= turn;
  }

  // Zoom.
  const float wheel = GetMouseWheelMove();
  if (!std::isfinite(wheel)) {
    throw std::runtime_error("Mouse wheel returned a non-finite value.");
  }
  if (wheel != 0.0f) {
    ZoomTowardCursor(wheel, targetY, camera);
  }

  // Pan, relative to where the camera looks: forward is away from the camera.
  float right = 0.0f;
  float forward = 0.0f;
  if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
    forward += 1.0f;
  }
  if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
    forward -= 1.0f;
  }
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
    right += 1.0f;
  }
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
    right -= 1.0f;
  }
  if (m_config.edgePanEnabled && IsWindowFocused() &&
      !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
    const Vector2 mouse = GetMousePosition();
    const float margin = m_config.edgePanMarginPixels;
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());
    if (mouse.x >= 0.0f && mouse.x <= width && mouse.y >= 0.0f &&
        mouse.y <= height) {
      right += (mouse.x >= width - margin ? 1.0f : 0.0f) -
               (mouse.x <= margin ? 1.0f : 0.0f);
      forward += (mouse.y <= margin ? 1.0f : 0.0f) -
                 (mouse.y >= height - margin ? 1.0f : 0.0f);
    }
  }

  const float inputLength = std::sqrt(right * right + forward * forward);
  if (inputLength > kMinimumVectorLength) {
    // Yaw 0 puts the camera on +z looking toward -z, with +x to the right.
    const float forwardX = -std::sin(m_current.yaw);
    const float forwardZ = -std::cos(m_current.yaw);
    const float rightX = std::cos(m_current.yaw);
    const float rightZ = -std::sin(m_current.yaw);
    const float step = m_config.panSpeedPerDistance * m_goal.distance *
                       deltaTime / inputLength;
    m_goal.targetX += (forwardX * forward + rightX * right) * step;
    m_goal.targetZ += (forwardZ * forward + rightZ * right) * step;
    ClampGoalTarget();
  }

  // Ease toward the goal.
  const float moveBlend = GetSmoothingBlend(m_config.moveSmoothing, deltaTime);
  m_current.targetX += (m_goal.targetX - m_current.targetX) * moveBlend;
  m_current.targetZ += (m_goal.targetZ - m_current.targetZ) * moveBlend;
  m_current.distance += (m_goal.distance - m_current.distance) * moveBlend;
  m_current.yaw += (m_goal.yaw - m_current.yaw) *
                   GetSmoothingBlend(m_config.yawSmoothing, deltaTime);

  camera = BuildCamera(m_current, targetY, camera);
}
}  // namespace Infinitesia
