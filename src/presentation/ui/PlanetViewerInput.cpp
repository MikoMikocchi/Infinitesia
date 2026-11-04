#include <algorithm>
#include <cmath>
#include <cstdio>

#include "presentation/ui/PlanetPalette.h"
#include "presentation/ui/PlanetViewer.h"
#include "presentation/ui/PlanetViewerLayout.h"
#include "raylib.h"
#include "raymath.h"

namespace Infinitesia {
void PlanetViewer::HandleInput() {
  const Vector2 mousePosition = GetMousePosition();
  const ViewerLayout layout =
      GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
  const int rows = m_planetMap.GetLatitudeBandCount();
  const int columns = m_planetMap.GetLongitudeSegmentCount();

  if (IsKeyPressed(KEY_R)) {
    if (m_regionMode) {
      m_regionMode = false;
    } else {
      EnterRegionMode();
    }
  }
  if (m_regionMode) {
    if (IsKeyPressed(KEY_BACKSPACE)) {
      m_regionMode = false;
    }
    if (m_foundingEnabled && IsKeyPressed(KEY_ENTER)) {
      TryFoundSettlement();
    }

    const auto pressed = [](int primary, int secondary) {
      return IsKeyPressed(primary) || IsKeyPressedRepeat(primary) ||
             IsKeyPressed(secondary) || IsKeyPressedRepeat(secondary);
    };
    const int stepX = (pressed(KEY_RIGHT, KEY_D) ? 1 : 0) -
                      (pressed(KEY_LEFT, KEY_A) ? 1 : 0);
    const int stepY =
        (pressed(KEY_UP, KEY_W) ? 1 : 0) - (pressed(KEY_DOWN, KEY_S) ? 1 : 0);
    if (stepX != 0 || stepY != 0) {
      // The map is north-up, so arrows move along the compass, one region at a
      // time.
      const RegionId focus = m_regionMap.GetFocus();
      m_regionMap.Focus({focus.i + stepX, focus.j + stepY});
      SyncSelectionToRegion();
    }
  }

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (m_regionMode && RectangleContains(layout.regionBounds, mousePosition)) {
      if (const auto direction =
              m_regionMap.DirectionAt(mousePosition, layout.regionBounds)) {
        m_regionMap.FocusAt(*direction);
        SyncSelectionToRegion();
      }
      m_draggingGlobe = false;
    } else if (!m_regionMode &&
               RectangleContains(layout.mapBounds, mousePosition)) {
      const float normalizedX =
          (mousePosition.x - layout.mapBounds.x) / layout.mapBounds.width;
      const float normalizedY =
          (mousePosition.y - layout.mapBounds.y) / layout.mapBounds.height;
      m_selectedLongitude =
          std::clamp(static_cast<int>(normalizedX * columns), 0, columns - 1);
      m_selectedLatitude =
          rows - 1 -
          std::clamp(static_cast<int>(normalizedY * rows), 0, rows - 1);
      m_draggingGlobe = false;
    } else {
      const Camera3D camera = MakeCamera();
      const Ray ray = GetMouseRay(mousePosition, camera);
      const RayCollision collision =
          GetRayCollisionSphere(ray, {0.0f, 0.0f, 0.0f}, 1.0f);
      // In region mode the globe is locked onto the focused region.
      m_draggingGlobe = collision.hit && !m_regionMode;
      m_lastMousePosition = mousePosition;

      if (collision.hit) {
        // Undo the transform the globe is drawn with, then flip z back (mesh
        // longitude runs opposite to geographic longitude).
        const Vector3 local = Vector3Transform(
            collision.point, MatrixInvert(GetGlobeTransform()));
        const GeographicCoordinates coordinates =
            PlanetaryCoordinates::ToGeographic({local.x, local.y, -local.z});
        m_selectedLatitude =
            std::clamp(static_cast<int>((coordinates.latitudeDegrees + 90.0) /
                                        180.0 * rows),
                       0, rows - 1);
        m_selectedLongitude = std::clamp(
            static_cast<int>(coordinates.longitudeDegrees / 360.0 * columns), 0,
            columns - 1);
        if (m_regionMode) {
          FocusRegionOnSelectedCell();
        }
      }
    }
  }

  if (m_draggingGlobe && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    const Vector2 delta{mousePosition.x - m_lastMousePosition.x,
                        mousePosition.y - m_lastMousePosition.y};
    m_rotationDegrees = std::fmod(m_rotationDegrees + delta.x * 0.35f, 360.0f);
    m_tiltDegrees = std::clamp(m_tiltDegrees + delta.y * 0.35f, -75.0f, 75.0f);
    m_lastMousePosition = mousePosition;
  }
  if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    m_draggingGlobe = false;
  }

  m_zoom = std::clamp(m_zoom - GetMouseWheelMove() * 0.1f,
                      GetMinimumZoom(layout, GetScreenHeight()), 2.0f);
}

}  // namespace Infinitesia
