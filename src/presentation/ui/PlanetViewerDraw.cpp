#include <algorithm>
#include <cmath>
#include <cstdio>

#include "presentation/ui/PlanetPalette.h"
#include "presentation/ui/PlanetViewer.h"
#include "presentation/ui/PlanetViewerLayout.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

namespace Infinitesia
{
void PlanetViewer::Draw() const
{
	const int screenWidth = GetScreenWidth();
	const int screenHeight = GetScreenHeight();
	const ViewerLayout layout = GetLayout(screenWidth, screenHeight, m_planetMap);
	const int rows = m_planetMap.GetLatitudeBandCount();
	const int columns = m_planetMap.GetLongitudeSegmentCount();

	ClearBackground({11, 17, 26, 255});

	DrawText("INFINITESIA  /  PLANET VIEWER", 28, 22, 24, RAYWHITE);
	char systemSummary[192];
	std::snprintf(systemSummary, sizeof(systemSummary),
	              "Star %.2f M_sun / %.2f L_sun  |  Age %.2f Gyr  |  Orbit %.2f "
	              "AU  |  Planet %.2f M_earth  |  Seed %llu  |  Gen v%u",
	              m_system.star.massKilograms / 1.98847e30, m_system.star.luminosityWatts / 3.828e26,
	              m_system.galacticContext.systemAgeYears / 1.0e9,
	              m_system.planetOrbit.semiMajorAxisMeters / 149597870700.0, m_system.planet.massKilograms / 5.9722e24,
	              static_cast<unsigned long long>(m_system.scenario.seed), m_system.scenario.generatorVersion);
	DrawText(systemSummary, 30, 52, 15, {139, 158, 177, 255});
	DrawFPS(screenWidth - 80, 24);

	DrawRectangleRounded(layout.mapPanel, 0.035f, 8, {19, 28, 40, 255});
	DrawRectangleRounded(layout.globePanel, 0.035f, 8, {19, 28, 40, 255});
	const char *mapTitle = m_regionMode ? "SITE MAP" : (m_worldMapMode ? "THEATER MAP" : "PLANET MAP");
	DrawText(mapTitle, static_cast<int>(layout.mapPanel.x + 18.0f), static_cast<int>(layout.mapPanel.y + 16.0f), 16,
	         SKYBLUE);
	DrawText("PLANET GLOBE", static_cast<int>(layout.globePanel.x + 18.0f),
	         static_cast<int>(layout.globePanel.y + 16.0f), 16, SKYBLUE);

	if (m_regionMode)
	{
		m_regionMap.Draw(layout.regionBounds);

		const RegionId focus = m_regionMap.GetFocus();
		const GeographicCoordinates focusCenter = m_regionMap.GetFocusCenter();
		const PlanetFieldSample sample = m_regionMap.GetTerrain().SampleAt(focusCenter);
		const double elevation = m_regionMap.GetTerrain().ElevationMeters(focusCenter, 16.0);
		const std::optional<Biome> biome = m_regionMap.FocusBiome();
		const LocalWater water = m_regionMap.GetTerrain().WaterAt(focusCenter, 16.0);
		const char *waterName = water.origin == FluidOrigin::Sea     ? "Sea"
		                        : water.origin == FluidOrigin::Lake  ? "Lake"
		                        : water.origin == FluidOrigin::River ? "River"
		                                                             : "None";
		const double spanKilometres =
		    (m_regionMap.PatchSpanMeters() > 0.0 ? m_regionMap.PatchSpanMeters() : LocalGrid::kRegionSizeMeters) /
		    1000.0;

		const float cardWidth = 350.0f;
		const float cardHeight = 102.0f;
		const float cardX =
		    std::max(layout.mapPanel.x + 16.0f, layout.mapPanel.x + layout.mapPanel.width - cardWidth - 16.0f);
		const float cardY = layout.mapPanel.y + 44.0f;
		DrawRectangleRounded({cardX, cardY, cardWidth, cardHeight}, 0.04f, 6, Fade({14, 21, 31, 255}, 0.90f));
		DrawRectangleLinesEx({cardX, cardY, cardWidth, cardHeight}, 1.0f, Fade({129, 158, 180, 255}, 0.45f));
		char cardLine[256];
		std::snprintf(cardLine, sizeof(cardLine), "Theater tile (%d, %d)  |  %.1f km across", focus.i, focus.j,
		              spanKilometres);
		DrawText(cardLine, static_cast<int>(cardX + 10.0f), static_cast<int>(cardY + 8.0f), 13, RAYWHITE);
		std::snprintf(cardLine, sizeof(cardLine), "%s  |  %.0f m", biome ? BiomeName(*biome) : "Unknown", elevation);
		DrawText(cardLine, static_cast<int>(cardX + 10.0f), static_cast<int>(cardY + 28.0f), 12, {180, 195, 210, 255});
		std::snprintf(cardLine, sizeof(cardLine), "%.1f C  |  %.0f mm/yr  |  Soil %.0f%%",
		              sample.surface.temperatureCelsius, sample.surface.precipitationMm,
		              sample.surface.soilFertility * 100.0);
		DrawText(cardLine, static_cast<int>(cardX + 10.0f), static_cast<int>(cardY + 48.0f), 12, {180, 195, 210, 255});
		std::snprintf(cardLine, sizeof(cardLine), "Water: %s  |  Province %u  |  Radius %d tiles", waterName,
		              sample.surface.province, m_regionMap.PatchRadius());
		DrawText(cardLine, static_cast<int>(cardX + 10.0f), static_cast<int>(cardY + 68.0f), 12, {180, 195, 210, 255});
	}
	else if (m_worldMapMode)
	{
		m_worldMapView.Draw(layout.worldBounds, m_regionMap.GetFocus());
	}
	else
	{
		const Rectangle textureSource{0.0f, 0.0f, static_cast<float>(columns), static_cast<float>(rows)};
		DrawTexturePro(m_surfaceTexture.Get(), textureSource, layout.mapBounds, {0.0f, 0.0f}, 0.0f, WHITE);
		const float cellWidth = layout.mapBounds.width / static_cast<float>(columns);
		const float cellHeight = layout.mapBounds.height / static_cast<float>(rows);
		const int selectedMapLatitude = rows - 1 - m_selectedLatitude;
		DrawRectangle(static_cast<int>(std::floor(layout.mapBounds.x + m_selectedLongitude * cellWidth)),
		              static_cast<int>(std::floor(layout.mapBounds.y + selectedMapLatitude * cellHeight)),
		              std::max(1, static_cast<int>(std::ceil(cellWidth))),
		              std::max(1, static_cast<int>(std::ceil(cellHeight))), {255, 202, 87, 255});

		for (int longitude = 0; longitude <= 12; ++longitude)
		{
			const float x = layout.mapBounds.x + layout.mapBounds.width * longitude / 12.0f;
			DrawLine(static_cast<int>(x), static_cast<int>(layout.mapBounds.y), static_cast<int>(x),
			         static_cast<int>(layout.mapBounds.y + layout.mapBounds.height), Fade(RAYWHITE, 0.12f));
		}
		for (int latitude = 0; latitude <= 6; ++latitude)
		{
			const float y = layout.mapBounds.y + layout.mapBounds.height * latitude / 6.0f;
			DrawLine(static_cast<int>(layout.mapBounds.x), static_cast<int>(y),
			         static_cast<int>(layout.mapBounds.x + layout.mapBounds.width), static_cast<int>(y),
			         Fade(RAYWHITE, 0.12f));
		}
		DrawRectangleLinesEx(layout.mapBounds, 1.0f, {129, 158, 180, 255});
	}

	const Camera3D camera = MakeCamera();
	BeginScissorMode(static_cast<int>(layout.globePanel.x), static_cast<int>(layout.globePanel.y),
	                 static_cast<int>(layout.globePanel.width), static_cast<int>(layout.globePanel.height));
	// At the theater/site depth the camera sits close to the surface; the default
	// near plane would clip the front of the globe.
	rlSetClipPlanes(0.05, 1000.0);
	BeginMode3D(camera);
	SyncGlobeMesh();
	const Matrix globeTransform = GetGlobeTransform();
	DrawMesh(m_globeMesh, m_globeMaterial, globeTransform);
	EndMode3D();
	rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
	if (m_worldMapMode || m_regionMode)
	{
		// The camera centres the selected point; this small cross shows exactly
		// where it is without painting a marker onto the surface.
		const float centreX = layout.globeCenter.x;
		const float centreY = layout.globeCenter.y;
		const Color cross = Fade(RAYWHITE, 0.55f);
		DrawLine(static_cast<int>(centreX - 9.0f), static_cast<int>(centreY),
		         static_cast<int>(centreX + 9.0f), static_cast<int>(centreY), cross);
		DrawLine(static_cast<int>(centreX), static_cast<int>(centreY - 9.0f),
		         static_cast<int>(centreX), static_cast<int>(centreY + 9.0f), cross);
	}
	EndScissorMode();

	const int footerY = screenHeight - 70;
	if (m_worldMapMode)
	{
		const auto world = m_regionMap.GetWorldMap();
		const RegionId focus = m_regionMap.GetFocus();
		char worldText[256];
		if (world)
		{
			const RegionTile *tile = world->Find(focus);
			if (tile != nullptr)
			{
				const char *water =
				    tile->IsOcean() ? "Ocean" : (tile->IsLake() ? "Lake" : (tile->HasRiver() ? "River" : "Dry"));
				std::snprintf(worldText, sizeof(worldText),
				              "Theater tile (%d, %d)  |  %.0f m  |  %.1f C  |  %.0f mm/yr  |  %s  |  %s", focus.i,
				              focus.j, static_cast<double>(tile->elevationMeters),
				              static_cast<double>(tile->temperatureCelsius), static_cast<double>(tile->precipitationMm),
				              BiomeName(tile->biome), water);
			}
			else
			{
				std::snprintf(worldText, sizeof(worldText), "Theater tile (%d, %d) is outside the theater", focus.i,
				              focus.j);
			}
		}
		else
		{
			std::snprintf(worldText, sizeof(worldText), "Theater map is not available");
		}
		DrawText(worldText, 30, footerY, 16, RAYWHITE);
		char worldDetail[256];
		std::snprintf(worldDetail, sizeof(worldDetail),
		              "Theater %d x %d tiles  |  %.0f km across  |  %.0f m per tile", kRegionTilesPerWorldSide,
		              kRegionTilesPerWorldSide, kWorldSizeMeters / 1000.0, kRegionTileSizeMeters);
		DrawText(worldDetail, 30, footerY + 20, 14, {180, 195, 210, 255});
		DrawText("Click a tile to focus  |  Double-click a tile to choose a site  |  "
		         "RMB/Esc: planet",
		         30, footerY + 40, 12, {100, 118, 140, 255});
		return;
	}

	char selectionText[256];
	const PlanetRegion &selectedRegion = m_planetMap.GetRegion(m_selectedLatitude, m_selectedLongitude);

	const char *biomeName = BiomeName(selectedRegion.biome);

	std::snprintf(selectionText, sizeof(selectionText),
	              "Planet cell %d, %d  |  %.1f deg, %.1f deg  |  %.2f km  |  %.1f C  |  %.0f "
	              "mm/yr  |  %s",
	              m_selectedLatitude, m_selectedLongitude, selectedRegion.latitudeDegrees,
	              selectedRegion.longitudeDegrees,
	              selectedRegion.elevationNormalized * kMaxElevationKm, // physical km
	              selectedRegion.temperatureCelsius, selectedRegion.precipitationMm, biomeName);

	DrawText(selectionText, 30, footerY, 16, RAYWHITE);

	// Second footer line: geological/hydrological details
	const char *rockName = [](RockType r) -> const char * {
		switch (r)
		{
		case RockType::BasalticOceanic:
			return "Basaltic";
		case RockType::GraniticShield:
			return "Granitic Shield";
		case RockType::SedimentaryBasin:
			return "Sedimentary";
		case RockType::VolcanicIgneous:
			return "Volcanic";
		case RockType::MetamorphicBelt:
			return "Metamorphic";
		case RockType::FoldedSedimentary:
			return "Folded Sedimentary";
		default:
			return "Unknown";
		}
	}(selectedRegion.surfaceRock);

	char detailText[512];
	if (selectedRegion.isLake)
	{
		std::snprintf(detailText, sizeof(detailText),
		              "Lake %.0f%% of cell (%.0f km2)  |  V %.3f km3  |  Annual "
		              "in/evap/infil/spill %.3f/%.3f/%.3f/%.3f km3/yr"
		              "  |  Level %.2f km  |  Spill height %.3f km",
		              selectedRegion.lakeFraction * 100.0, selectedRegion.lakeSurfaceAreaKm2,
		              selectedRegion.lakeWaterVolumeKm3, selectedRegion.lakeAnnualInflowKm3,
		              selectedRegion.lakeAnnualEvaporationKm3, selectedRegion.lakeAnnualInfiltrationKm3,
		              selectedRegion.lakeAnnualSpillKm3, selectedRegion.lakeWaterLevelKm,
		              selectedRegion.lakeSpillHeightKm);
	}
	else
	{
		std::snprintf(detailText, sizeof(detailText), "%s  |  %s  |  Stress %.2f  |  %s  |  Soil %.0f%%  |  River: %s",
		              rockName, selectedRegion.isVolcanicZone ? "Volcanic" : "Stable", selectedRegion.tectonicStress,
		              selectedRegion.isOcean ? "Ocean" : "Land", selectedRegion.soilFertility * 100.0,
		              selectedRegion.hasRiver ? "Yes" : "No");
	}
	DrawText(detailText, 30, footerY + 20, 14, {180, 195, 210, 255});

	if (m_regionMode)
	{
		const RegionId focus = m_regionMap.GetFocus();
		const GeographicCoordinates focusCenter = m_regionMap.GetFocusCenter();
		char regionText[256];
		const auto cursorElevation = m_regionMap.ElevationAt(GetMousePosition(), layout.regionBounds);
		const auto cursorBiome = m_regionMap.BiomeAt(GetMousePosition(), layout.regionBounds);
		if (cursorElevation && cursorBiome)
		{
			std::snprintf(regionText, sizeof(regionText),
			              "Theater tile (%d, %d)  |  %.3f, %.3f deg  |  "
			              "Cursor: %.0f m, %s",
			              focus.i, focus.j, focusCenter.latitudeDegrees, focusCenter.longitudeDegrees, *cursorElevation,
			              BiomeName(*cursorBiome));
		}
		else
		{
			std::snprintf(
			    regionText, sizeof(regionText), "Theater tile (%d, %d)  |  %.3f, %.3f deg  |  %.1f km across", focus.i,
			    focus.j, focusCenter.latitudeDegrees, focusCenter.longitudeDegrees,
			    (m_regionMap.PatchSpanMeters() > 0.0 ? m_regionMap.PatchSpanMeters() : LocalGrid::kRegionSizeMeters) /
			        1000.0);
		}
		DrawText(regionText, 30, footerY + 40, 13, kSelectedCellColor);
		DrawText(m_foundingEnabled ? "Wheel: zoom  |  Click / WASD / arrows: pick a site  |  "
		                             "Enter: found  |  RMB/Esc: theater"
		                           : "Wheel: zoom  |  Click / WASD / arrows: pick a site  |  "
		                             "M: settlement  |  RMB/Esc: theater",
		         30, footerY + 57, 12, {100, 118, 140, 255});
		if (m_noticeSeconds > 0.0f)
		{
			DrawText(m_notice.c_str(), static_cast<int>(layout.regionBounds.x),
			         static_cast<int>(layout.mapPanel.y + 16.0f), 16, {255, 120, 90, 255});
		}
		return;
	}

	char seasonalText[256];
	constexpr int kSeasonMonths[4] = {0, 3, 6, 9};
	std::snprintf(seasonalText, sizeof(seasonalText),
	              "Jan %.0f C / %.0f mm  |  Apr %.0f C / %.0f mm  |  Jul %.0f C "
	              "/ %.0f mm  |  Oct %.0f C / %.0f mm",
	              selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[0]],
	              selectedRegion.monthlyPrecipitationMm[kSeasonMonths[0]],
	              selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[1]],
	              selectedRegion.monthlyPrecipitationMm[kSeasonMonths[1]],
	              selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[2]],
	              selectedRegion.monthlyPrecipitationMm[kSeasonMonths[2]],
	              selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[3]],
	              selectedRegion.monthlyPrecipitationMm[kSeasonMonths[3]]);
	DrawText(seasonalText, 30, footerY + 40, 13, {100, 118, 140, 255});
	DrawText("Click the map or globe to open a theater  |  Drag the globe to rotate  |  Scroll to zoom",
	         30, footerY + 57, 12, {100, 118, 140, 255});
}

} // namespace Infinitesia
