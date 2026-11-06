#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/LoadProgress.h"
#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/StarSystem.h"
#include "domains/natural/planetary/pipeline/PlanetPipeline.h"
#include "domains/natural/planetary/region/Site.h"
#include "domains/natural/planetary/region/WorldTerrain.h"
#include "presentation/ui/RegionMapView.h"
#include "presentation/ui/UniqueTexture.h"
#include "presentation/ui/WorldMapView.h"
#include "raylib.h"
#include "raymath.h"

namespace Infinitesia
{
class PlanetViewer
{
public:
	explicit PlanetViewer(StarSystem system, LoadProgress *progress = nullptr);
	// The planet is built by `pipeline` (the default stages plus those of the
	// domains the game uses).
	PlanetViewer(StarSystem system, const PlanetPipeline &pipeline, LoadProgress *progress = nullptr);

	void Update();
	void Draw() const;
	void ReleaseRenderResources() noexcept;

	// Opens the theater at a point. Used by the dev screenshot hooks and by the
	// normal planet click path.
	void OpenTheaterAt(const GeographicCoordinates &point);
	// Opens the site map for the theater at a point.
	void OpenSiteAt(const GeographicCoordinates &point);

	// Set when the player founds a site; handed over once.
	std::optional<Site> TakeFoundedSite();
	void SetFoundingEnabled(bool enabled) noexcept
	{
		m_foundingEnabled = enabled;
	}
	const WorldTerrain &GetRegionTerrain() const noexcept
	{
		return m_regionMap.GetTerrain();
	}

private:
	void HandleInput();
	void EnsureSurfaceTexture();
	Camera3D MakeCamera() const;
	Matrix GetGlobeTransform() const;
	void SyncGlobeMesh() const;
	void WriteCellColors(Color *out, int row, int column, bool selected) const;
	// Anchors the theater at the selected planet cell and opens the theater map.
	void EnterTheaterFromSelectedCell();
	// Opens the site map for the current theater tile.
	void EnterSiteMode();
	// Goes one level up: site -> theater -> planet.
	void GoBackOneLevel();
	void RotateGlobe(float yawDegrees, float pitchDegrees);
	void TryFoundSettlement();
	// A dry point inside (or very near) the tile, preferably close to water.
	std::optional<GeographicCoordinates> FindDrySite(const LocalGrid &grid, RegionId tile) const;
	void SyncSelectionToRegion();

	StarSystem m_system;
	PlanetMap m_planetMap;
	RegionMapView m_regionMap;
	WorldMapView m_worldMapView;
	std::vector<Color> m_regionColors;
	std::vector<float> m_regionBrightness;
	std::vector<Vector3> m_sphereVertices;
	std::vector<Color> m_sphereVertexColors;
	std::vector<float> m_sphereVertexBrightness;
	UniqueTexture m_surfaceTexture;
	// Static globe mesh (6 vertices per cell, baked vertex colors), built lazily
	// once a GL context exists. Rotation is applied via a transform matrix.
	mutable Mesh m_globeMesh{};
	mutable Material m_globeMaterial{};
	mutable int m_uploadedSelectedCell = -1;
	int m_selectedLatitude = 0;
	int m_selectedLongitude = 0;
	float m_rotationDegrees = 0.0f;
	float m_tiltDegrees = -12.0f;
	float m_zoom = 1.0f;
	bool m_foundingEnabled = true;
	std::optional<Site> m_foundedSite;
	std::string m_notice;
	float m_noticeSeconds = 0.0f;
	// The game starts on the planet; a click opens the theater, a double-click
	// on a theater tile opens the site map.
	bool m_regionMode = false;
	bool m_worldMapMode = false;
	float m_levelBlend = 0.0f; // 0 = planet, 1 = theater, 2 = site
	// The globe has one continuous orientation. Entering a region slerps it to
	// face the selected point; leaving a region keeps the current orientation
	// and resumes free rotation from there.
	Quaternion m_globeOrientation = {0.0f, 0.0f, 0.0f, 1.0f};
	Quaternion m_orientationTarget = {0.0f, 0.0f, 0.0f, 1.0f};
	bool m_hasOrientationTarget = false;
	bool m_draggingGlobe = false;
	bool m_draggingMap = false;
	unsigned m_regionGridVersion = 0;
	Vector2 m_lastMousePosition{};
	// A short click on the globe opens the theater; a drag only rotates.
	Vector2 m_globePressPosition{};
	GeographicCoordinates m_globePressCoordinates{};
	bool m_globePressHit = false;
	bool m_globeDragged = false;
	// Double-click on the theater map selects the site region.
	double m_lastTheaterClickTime = -1.0;
	RegionTileId m_lastTheaterClickTile{};
	bool m_hasTheaterClick = false;
};
} // namespace Infinitesia
