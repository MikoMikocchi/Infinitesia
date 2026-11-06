#include <algorithm>
#include <cmath>
#include <cstdio>

#include "presentation/ui/PlanetPalette.h"
#include "presentation/ui/PlanetViewer.h"
#include "presentation/ui/PlanetViewerLayout.h"
#include "raylib.h"
#include "raymath.h"

namespace Infinitesia
{
void PlanetViewer::HandleInput()
{
	const Vector2 mousePosition = GetMousePosition();
	const ViewerLayout layout = GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();

	// One level up: site -> theater -> planet. Right click, Esc or Backspace.
	if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) ||
	    IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
	{
		if (m_regionMode || m_worldMapMode)
		{
			GoBackOneLevel();
			return;
		}
	}

	if (m_regionMode)
	{
		if (m_foundingEnabled && IsKeyPressed(KEY_ENTER))
		{
			TryFoundSettlement();
		}

		const auto pressed = [](int primary, int secondary) {
			return IsKeyPressed(primary) || IsKeyPressedRepeat(primary) || IsKeyPressed(secondary) ||
			       IsKeyPressedRepeat(secondary);
		};
		const int stepX = (pressed(KEY_RIGHT, KEY_D) ? 1 : 0) - (pressed(KEY_LEFT, KEY_A) ? 1 : 0);
		const int stepY = (pressed(KEY_UP, KEY_W) ? 1 : 0) - (pressed(KEY_DOWN, KEY_S) ? 1 : 0);
		if (stepX != 0 || stepY != 0)
		{
			// The map is north-up, so arrows move along the compass, one tile at a
			// time.
			const RegionId focus = m_regionMap.GetFocus();
			m_regionMap.Focus({focus.i + stepX, focus.j + stepY});
			SyncSelectionToRegion();
		}
	}

	if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		m_globePressHit = false;
		m_globeDragged = false;

		if (m_regionMode && RectangleContains(layout.regionBounds, mousePosition))
		{
			if (const auto direction = m_regionMap.DirectionAt(mousePosition, layout.regionBounds))
			{
				m_regionMap.FocusAt(*direction);
				SyncSelectionToRegion();
			}
			m_draggingGlobe = false;
		}
		else if (m_worldMapMode && RectangleContains(layout.worldBounds, mousePosition))
		{
			if (const auto tile = m_worldMapView.TileAt(mousePosition, layout.worldBounds))
			{
				const double now = GetTime();
				const bool doubleClick = m_hasTheaterClick && now - m_lastTheaterClickTime <= 0.35 &&
				                         tile->i == m_lastTheaterClickTile.i && tile->j == m_lastTheaterClickTile.j;

				m_regionMap.Focus({tile->i, tile->j});
				SyncSelectionToRegion();

				if (doubleClick)
				{
					EnterSiteMode();
				}
				else
				{
					m_lastTheaterClickTime = now;
					m_lastTheaterClickTile = *tile;
					m_hasTheaterClick = true;
				}
			}
			m_draggingGlobe = false;
		}
		else if (!m_regionMode && !m_worldMapMode && RectangleContains(layout.mapBounds, mousePosition))
		{
			const float normalizedX = (mousePosition.x - layout.mapBounds.x) / layout.mapBounds.width;
			const float normalizedY = (mousePosition.y - layout.mapBounds.y) / layout.mapBounds.height;
			m_selectedLongitude = std::clamp(static_cast<int>(normalizedX * columns), 0, columns - 1);
			m_selectedLatitude = rows - 1 - std::clamp(static_cast<int>(normalizedY * rows), 0, rows - 1);
			EnterTheaterFromSelectedCell();
			m_draggingGlobe = false;
		}
		else if (!m_regionMode && !m_worldMapMode && RectangleContains(layout.globePanel, mousePosition))
		{
			// On the planet the globe is a click target. A press starts a
			// potential drag; a release without movement opens the theater at
			// that point. In the theater/site views the globe is only context.
			const Camera3D camera = MakeCamera();
			const Ray ray = GetMouseRay(mousePosition, camera);
			const RayCollision collision = GetRayCollisionSphere(ray, {0.0f, 0.0f, 0.0f}, 1.0f);
			if (collision.hit)
			{
				const Vector3 local = Vector3Transform(collision.point, MatrixInvert(GetGlobeTransform()));
				m_globePressCoordinates = PlanetaryCoordinates::ToGeographic({local.x, local.y, -local.z});
				m_globePressPosition = mousePosition;
				m_globePressHit = true;
				m_draggingGlobe = !m_worldMapMode;
				m_lastMousePosition = mousePosition;
			}
		}
	}

	if (m_draggingGlobe && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
	{
		const Vector2 delta{mousePosition.x - m_lastMousePosition.x, mousePosition.y - m_lastMousePosition.y};
		if (Vector2Distance(mousePosition, m_globePressPosition) > 4.0f)
		{
			m_globeDragged = true;
		}
		if (m_globeDragged)
		{
			RotateGlobe(delta.x * 0.35f, delta.y * 0.35f);
		}
		m_lastMousePosition = mousePosition;
	}

	if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
	{
		if (m_globePressHit && !m_globeDragged && !m_regionMode)
		{
			m_selectedLatitude =
			    std::clamp(static_cast<int>((m_globePressCoordinates.latitudeDegrees + 90.0) / 180.0 * rows), 0,
			               rows - 1);
			m_selectedLongitude =
			    std::clamp(static_cast<int>(m_globePressCoordinates.longitudeDegrees / 360.0 * columns), 0,
			               columns - 1);
			EnterTheaterFromSelectedCell();
		}
		m_globePressHit = false;
		m_globeDragged = false;
		m_draggingGlobe = false;
	}

	const float wheel = GetMouseWheelMove();
	if (m_regionMode)
	{
		if (wheel != 0.0f)
		{
			m_regionMap.Zoom(wheel > 0.0f ? 1 : -1);
		}
	}
	else
	{
		m_zoom = std::clamp(m_zoom - wheel * 0.1f, GetMinimumZoom(layout, GetScreenHeight()), 2.0f);
	}
}

} // namespace Infinitesia
