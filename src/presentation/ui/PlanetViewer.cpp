#include "presentation/ui/PlanetViewer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

#include "domains/natural/planetary/PlanetLoadSteps.h"
#include "domains/natural/planetary/region/SettlementTerrain.h"
#include "presentation/ui/PlanetGlobeGeometry.h"
#include "presentation/ui/PlanetPalette.h"
#include "presentation/ui/PlanetViewerLayout.h"
#include "raylib.h"
#include "raymath.h"

namespace Infinitesia
{
PlanetViewer::PlanetViewer(StarSystem system, LoadProgress *progress)
    : PlanetViewer(std::move(system), MakeDefaultPlanetPipeline(), progress)
{
}

PlanetViewer::PlanetViewer(StarSystem system, const PlanetPipeline &pipeline, LoadProgress *progress)
    : m_system(std::move(system)), m_planetMap(m_system.planet, ClimateInputsFor(m_system), pipeline, progress),
      m_regionMap(m_planetMap,
	              [](const PlanetRegion &cell, float elevationMeters) {
	                  // Tint by the local elevation, not the cell average, and
	                  // skip the cell-wide lake wash: at this scale a lake is not
	                  // the whole cell.
	                  PlanetRegion local = cell;
	                  local.elevationNormalized =
	                      std::clamp(static_cast<double>(elevationMeters) / (kMaxElevationKm * 1000.0), -1.0, 1.0);
	                  local.isLake = false;
	                  return GetRegionColor(local);
                  }),
      m_selectedLatitude(m_planetMap.GetLatitudeBandCount() / 2),
      m_selectedLongitude(m_planetMap.GetLongitudeSegmentCount() / 2)
{
	BeginLoadStep(progress, LoadStep::kViewerColors, "Preparing surface colors");
	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();
	const std::size_t cellCount = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
	m_regionColors.reserve(cellCount);
	m_regionBrightness.reserve(cellCount);
	const Vector3 lightDirection = Vector3Normalize(Vector3{-0.45f, 0.62f, -0.85f});

	for (int latitude = 0; latitude < rows; ++latitude)
	{
		for (int longitude = 0; longitude < columns; ++longitude)
		{
			const PlanetRegion &region = m_planetMap.GetRegion(latitude, longitude);
			m_regionColors.push_back(GetRegionColor(region));
			const float latitudeRadians = static_cast<float>(region.latitudeDegrees) * kPi / 180.0f;
			const float longitudeRadians = static_cast<float>(region.longitudeDegrees) * kPi / 180.0f;
			const Vector3 normal{std::cos(latitudeRadians) * std::cos(longitudeRadians), std::sin(latitudeRadians),
			                     -std::cos(latitudeRadians) * std::sin(longitudeRadians)};
			const int northLatitude = std::min(latitude + 1, rows - 1);
			const int southLatitude = std::max(latitude - 1, 0);
			const int eastLongitude = (longitude + 1) % columns;
			const int westLongitude = (longitude + columns - 1) % columns;
			const double northElevation =
			    m_planetMap.GetRegion(northLatitude, longitude).elevationNormalized * kMaxElevationKm;
			const double southElevation =
			    m_planetMap.GetRegion(southLatitude, longitude).elevationNormalized * kMaxElevationKm;
			const double eastElevation =
			    m_planetMap.GetRegion(latitude, eastLongitude).elevationNormalized * kMaxElevationKm;
			const double westElevation =
			    m_planetMap.GetRegion(latitude, westLongitude).elevationNormalized * kMaxElevationKm;
			const float reliefShade = std::clamp(1.0f + static_cast<float>((southElevation - northElevation) * 0.12 +
			                                                               (westElevation - eastElevation) * 0.12),
			                                     0.78f, 1.22f);
			const float sphericalShade = 0.24f + 0.76f * std::max(0.0f, Vector3DotProduct(normal, lightDirection));
			m_regionBrightness.push_back(sphericalShade * reliefShade);
		}
	}

	BeginLoadStep(progress, LoadStep::kViewerSmoothing, "Smoothing biome colors");
	const std::vector<Color> biomeColors = m_regionColors;
	constexpr int kColorOffsets[3] = {-1, 0, 1};
	constexpr int kColorWeights[3][3] = {{1, 1, 1}, {1, 8, 1}, {1, 1, 1}};
	for (int latitude = 0; latitude < rows; ++latitude)
	{
		for (int longitude = 0; longitude < columns; ++longitude)
		{
			std::size_t red = 0;
			std::size_t green = 0;
			std::size_t blue = 0;
			std::size_t totalWeight = 0;
			for (int latitudeOffset : kColorOffsets)
			{
				for (int longitudeOffset : kColorOffsets)
				{
					const int neighborLatitude = std::clamp(latitude + latitudeOffset, 0, rows - 1);
					const int neighborLongitude = (longitude + longitudeOffset + columns) % columns;
					const std::size_t neighborIndex =
					    static_cast<std::size_t>(neighborLatitude) * columns + neighborLongitude;
					const std::size_t weight =
					    static_cast<std::size_t>(kColorWeights[latitudeOffset + 1][longitudeOffset + 1]);
					const Color color = biomeColors[neighborIndex];
					red += color.r * weight;
					green += color.g * weight;
					blue += color.b * weight;
					totalWeight += weight;
				}
			}
			const std::size_t index = static_cast<std::size_t>(latitude) * columns + longitude;
			m_regionColors[index] = {static_cast<unsigned char>(red / totalWeight),
			                         static_cast<unsigned char>(green / totalWeight),
			                         static_cast<unsigned char>(blue / totalWeight), 255};
		}
	}

	const std::vector<float> cellBrightness = m_regionBrightness;
	for (int latitude = 0; latitude < rows; ++latitude)
	{
		for (int longitude = 0; longitude < columns; ++longitude)
		{
			float brightnessSum = 0.0f;
			int weightSum = 0;
			for (int latitudeOffset : kColorOffsets)
			{
				for (int longitudeOffset : kColorOffsets)
				{
					const int neighborLatitude = std::clamp(latitude + latitudeOffset, 0, rows - 1);
					const int neighborLongitude = (longitude + longitudeOffset + columns) % columns;
					const int weight = kColorWeights[latitudeOffset + 1][longitudeOffset + 1];
					brightnessSum +=
					    cellBrightness[static_cast<std::size_t>(neighborLatitude) * columns + neighborLongitude] *
					    weight;
					weightSum += weight;
				}
			}
			m_regionBrightness[static_cast<std::size_t>(latitude) * columns + longitude] =
			    brightnessSum / static_cast<float>(weightSum);
		}
	}

	m_sphereVertices.reserve(static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
	m_sphereVertexColors.reserve(static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
	m_sphereVertexBrightness.reserve(static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
	for (int latitude = 0; latitude <= rows; ++latitude)
	{
		for (int longitude = 0; longitude <= columns; ++longitude)
		{
			const int northCell = std::clamp(rows - 1 - latitude, 0, rows - 1);
			const int southCell = std::clamp(rows - latitude, 0, rows - 1);
			const int westCell = (longitude + columns - 1) % columns;
			const int eastCell = longitude % columns;
			const double meanElevationKm = (m_planetMap.GetRegion(northCell, westCell).elevationNormalized +
			                                m_planetMap.GetRegion(northCell, eastCell).elevationNormalized +
			                                m_planetMap.GetRegion(southCell, westCell).elevationNormalized +
			                                m_planetMap.GetRegion(southCell, eastCell).elevationNormalized) *
			                               kMaxElevationKm / 4.0;
			const double planetRadiusMeters = m_planetMap.GetParameters().radiusMeters;
			const float displayRadius = static_cast<float>(1.0 + meanElevationKm * 1000.0 / planetRadiusMeters * 8.0);
			m_sphereVertices.push_back(GetSphereVertex(latitude, longitude, rows, columns, displayRadius));
			const Color adjacentColors[4] = {m_regionColors[static_cast<std::size_t>(northCell) * columns + westCell],
			                                 m_regionColors[static_cast<std::size_t>(northCell) * columns + eastCell],
			                                 m_regionColors[static_cast<std::size_t>(southCell) * columns + westCell],
			                                 m_regionColors[static_cast<std::size_t>(southCell) * columns + eastCell]};
			m_sphereVertexColors.push_back(AverageColors(adjacentColors, 4));
			const float adjacentBrightness[4] = {
			    m_regionBrightness[static_cast<std::size_t>(northCell) * columns + westCell],
			    m_regionBrightness[static_cast<std::size_t>(northCell) * columns + eastCell],
			    m_regionBrightness[static_cast<std::size_t>(southCell) * columns + westCell],
			    m_regionBrightness[static_cast<std::size_t>(southCell) * columns + eastCell]};
			m_sphereVertexBrightness.push_back(
			    (adjacentBrightness[0] + adjacentBrightness[1] + adjacentBrightness[2] + adjacentBrightness[3]) *
			    0.25f);
		}
	}
	// Start on a land world, not on the middle of an ocean. Prefer a river
	// tile near the planet centre; fall back to the first land cell.
	int bestLatitude = rows / 2;
	int bestLongitude = columns / 2;
	double bestScore = -1.0;
	for (int latitude = 0; latitude < rows; ++latitude)
	{
		for (int longitude = 0; longitude < columns; ++longitude)
		{
			const PlanetRegion &region = m_planetMap.GetRegion(latitude, longitude);
			if (region.isOcean || region.isLake)
			{
				continue;
			}
			const double centrality =
			    1.0 - static_cast<double>(std::abs(latitude - rows / 2) + std::abs(longitude - columns / 2)) /
			              static_cast<double>(rows + columns);
			const double score = (region.hasRiver ? 4.0 : 0.0) + region.precipitationMm / 1000.0 +
			                     region.soilFertility * 2.0 + centrality;
			if (score > bestScore)
			{
				bestScore = score;
				bestLatitude = latitude;
				bestLongitude = longitude;
			}
		}
	}
	if (bestScore >= 0.0)
	{
		m_selectedLatitude = bestLatitude;
		m_selectedLongitude = bestLongitude;
		m_regionMap.Anchor(PlanetaryCoordinates::ToCartesian(
		    m_planetMap.GetPlanet().GetRegionCenter(m_selectedLatitude, m_selectedLongitude)));
	}

	m_regionGridVersion = m_regionMap.GridVersion();
	m_globeOrientation = QuaternionFromMatrix(
	    MatrixMultiply(MatrixRotateX(m_tiltDegrees * DEG2RAD), MatrixRotateY(m_rotationDegrees * DEG2RAD)));
}

void PlanetViewer::Update()
{
	EnsureSurfaceTexture();
	m_regionMap.Update();
	if (m_regionGridVersion != m_regionMap.GridVersion())
	{
		m_regionGridVersion = m_regionMap.GridVersion();
		SyncSelectionToRegion();
		m_hasTheaterClick = false;
	}
	m_worldMapView.SetWorld(m_regionMap.GetWorldMap());
	m_worldMapView.Update();
	HandleInput();

	const float frameTime = GetFrameTime();
	m_noticeSeconds = std::max(0.0f, m_noticeSeconds - frameTime);
	// Do not start the camera move while the background workers are still
	// building the next theater or site patch. The old image is hidden behind a
	// loading card, so the transition starts when the data is ready instead of
	// competing with generation for the main thread.
	const bool waiting =
	    (m_worldMapMode && (m_regionMap.IsAnchoring() || m_worldMapView.IsLoading())) ||
	    (m_regionMode && m_regionMap.IsLoading());
	const float levelTarget = waiting ? m_levelBlend : (m_regionMode ? 2.0f : (m_worldMapMode ? 1.0f : 0.0f));
	m_levelBlend += (levelTarget - m_levelBlend) * std::min(1.0f, frameTime * 4.0f);
	if (std::abs(levelTarget - m_levelBlend) < 0.002f)
	{
		m_levelBlend = levelTarget;
	}

	if (m_hasOrientationTarget && !waiting)
	{
		// Smoothly turn the globe to the selected place. The same path is used
		// when entering a theater after a previous visit, so it never jumps.
		m_globeOrientation = QuaternionNormalize(
		    QuaternionSlerp(m_globeOrientation, m_orientationTarget, std::min(1.0f, frameTime * 6.0f)));
	}
	else if (!m_draggingGlobe && !m_worldMapMode && !m_regionMode)
	{
		// Free globe: keep drifting from the current orientation, never from a
		// remembered camera position.
		const Quaternion spin = QuaternionFromAxisAngle({0.0f, 1.0f, 0.0f}, frameTime * 8.0f * DEG2RAD);
		m_globeOrientation = QuaternionNormalize(QuaternionMultiply(m_globeOrientation, spin));
	}
}

std::optional<GeographicCoordinates> PlanetViewer::FindDrySite(const LocalGrid &grid, RegionId tile) const
{
	const WorldTerrain &terrain = m_regionMap.GetTerrain();
	constexpr double kStepMeters = 24.0;
	constexpr int kHalfSteps = 8; // +/- 192 m, inside the 1024 m tile
	const double centerEast = static_cast<double>(tile.i) * kRegionTileSizeMeters;
	const double centerNorth = static_cast<double>(tile.j) * kRegionTileSizeMeters;

	const auto wetAt = [&](const GeographicCoordinates &position) {
		const double elevation = terrain.ElevationMeters(position, 16.0);
		const LocalWater water = terrain.WaterAt(position, 16.0);
		if (water.origin == FluidOrigin::Sea || water.origin == FluidOrigin::River)
		{
			return true;
		}
		if (water.origin == FluidOrigin::Lake)
		{
			return elevation < water.surfaceMeters;
		}
		return false;
	};

	static constexpr int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
	static constexpr int kDz[8] = {0, 0, 1, -1, 1, -1, 1, -1};
	std::optional<GeographicCoordinates> best;
	double bestScore = std::numeric_limits<double>::infinity();
	for (int northStep = -kHalfSteps; northStep <= kHalfSteps; ++northStep)
	{
		for (int eastStep = -kHalfSteps; eastStep <= kHalfSteps; ++eastStep)
		{
			const double east = centerEast + static_cast<double>(eastStep) * kStepMeters;
			const double north = centerNorth + static_cast<double>(northStep) * kStepMeters;
			const GeographicCoordinates candidate = PlanetaryCoordinates::ToGeographic(grid.Direction(east, north));
			if (terrain.ElevationMeters(candidate, 16.0) <= 0.0 || wetAt(candidate))
			{
				continue;
			}

			// Prefer a dry point with water close by: a river bank or a lake shore.
			double nearestWater = std::numeric_limits<double>::infinity();
			for (int direction = 0; direction < 8 && nearestWater > kStepMeters; ++direction)
			{
				for (int radius = 1; radius <= 6 && nearestWater > kStepMeters; ++radius)
				{
					const double sampleEast = east + static_cast<double>(kDx[direction] * radius) * kStepMeters;
					const double sampleNorth = north + static_cast<double>(kDz[direction] * radius) * kStepMeters;
					const GeographicCoordinates sample =
					    PlanetaryCoordinates::ToGeographic(grid.Direction(sampleEast, sampleNorth));
					if (wetAt(sample))
					{
						nearestWater = static_cast<double>(radius) * kStepMeters;
					}
				}
			}

			const double distanceToCenter = std::hypot(static_cast<double>(eastStep), static_cast<double>(northStep));
			const double score =
			    nearestWater < std::numeric_limits<double>::infinity() ? nearestWater : 100000.0 + distanceToCenter;
			if (score < bestScore)
			{
				bestScore = score;
				best = candidate;
			}
		}
	}
	return best;
}

std::optional<Site> PlanetViewer::TakeFoundedSite()
{
	std::optional<Site> site;
	site.swap(m_foundedSite);
	return site;
}

void PlanetViewer::TryFoundSettlement()
{
	const LocalGrid &grid = m_regionMap.GetGrid();
	const RegionId focus = m_regionMap.GetFocus();
	const std::optional<GeographicCoordinates> dry = FindDrySite(grid, focus);
	if (!dry.has_value())
	{
		m_notice = "No dry land in this tile - pick another region.";
		m_noticeSeconds = 4.0f;
		return;
	}

	Site site;
	site.kind = SiteKind::Settlement;
	site.grid = grid;
	site.region = focus;
	site.center = *dry;
	site.frame = LocalFrame::At(site.center);
	m_foundedSite = site;
}

void PlanetViewer::OpenTheaterAt(const GeographicCoordinates &point)
{
	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();
	m_selectedLatitude =
	    std::clamp(static_cast<int>((point.latitudeDegrees + 90.0) / 180.0 * rows), 0, rows - 1);
	m_selectedLongitude =
	    std::clamp(static_cast<int>(point.longitudeDegrees / 360.0 * columns), 0, columns - 1);
	EnterTheaterFromSelectedCell();
}

void PlanetViewer::OpenSiteAt(const GeographicCoordinates &point)
{
	OpenTheaterAt(point);
	EnterSiteMode();
}

void PlanetViewer::RotateGlobe(float yawDegrees, float pitchDegrees)
{
	m_hasOrientationTarget = false;
	const Quaternion yaw = QuaternionFromAxisAngle({0.0f, 1.0f, 0.0f}, yawDegrees * DEG2RAD);
	const Quaternion pitch = QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, pitchDegrees * DEG2RAD);
	m_globeOrientation = QuaternionNormalize(
	    QuaternionMultiply(m_globeOrientation, QuaternionMultiply(yaw, pitch)));
}

void PlanetViewer::EnterTheaterFromSelectedCell()
{
	// Reuse the current theater when the click is inside it. Only a click outside
	// starts a new theater, and that generation runs on a worker.
	const GeographicCoordinates target =
	    m_planetMap.GetPlanet().GetRegionCenter(m_selectedLatitude, m_selectedLongitude);
	const CartesianPoint3D direction = PlanetaryCoordinates::ToCartesian(target);

	GeographicCoordinates focus = target;
	bool focusedLocal = false;
	if (!m_regionMap.IsAnchoring())
	{
		if (const auto world = m_regionMap.GetWorldMap(); world && world->Find(direction))
		{
			m_regionMap.FocusAt(direction);
			focus = m_regionMap.GetFocusCenter();
			focusedLocal = true;
		}
		else
		{
			m_regionMap.AnchorAsync(direction);
		}
	}
	else
	{
		m_regionMap.AnchorAsync(direction);
	}

	m_orientationTarget = QuaternionFromMatrix(MakeFacingMatrix(focus));
	m_hasOrientationTarget = true;
	m_worldMapMode = true;
	m_regionMode = false;
	m_hasTheaterClick = false;
	if (focusedLocal)
	{
		SyncSelectionToRegion();
	}
}

void PlanetViewer::EnterSiteMode()
{
	if (!m_worldMapMode)
	{
		return;
	}
	// The theater tile stays focused; the site map is its close-up.
	m_worldMapMode = false;
	m_regionMode = true;
	m_hasTheaterClick = false;
}

void PlanetViewer::GoBackOneLevel()
{
	if (m_regionMode)
	{
		m_regionMode = false;
		m_worldMapMode = true;
	}
	else if (m_worldMapMode)
	{
		m_worldMapMode = false;
		// Pull back from the current orientation; never fly to the old one.
		m_hasOrientationTarget = false;
	}
	m_hasTheaterClick = false;
}

void PlanetViewer::SyncSelectionToRegion()
{
	const GeographicCoordinates center = m_regionMap.GetFocusCenter();
	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();
	m_selectedLatitude = std::clamp(static_cast<int>((center.latitudeDegrees + 90.0) / 180.0 * rows), 0, rows - 1);
	m_selectedLongitude = std::clamp(static_cast<int>(center.longitudeDegrees / 360.0 * columns), 0, columns - 1);
	// Any focus change (click, drag, new theater) turns the globe toward it
	// smoothly instead of leaving the camera beside the selected place.
	m_orientationTarget = QuaternionFromMatrix(MakeFacingMatrix(center));
	m_hasOrientationTarget = true;
}

} // namespace Infinitesia
