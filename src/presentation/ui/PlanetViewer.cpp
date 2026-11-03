#include "presentation/ui/PlanetViewer.h"

#include "domains/natural/planetary/PlanetLoadSteps.h"
#include "presentation/ui/PlanetGlobeGeometry.h"
#include "presentation/ui/PlanetPalette.h"
#include "presentation/ui/PlanetViewerLayout.h"
#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    PlanetViewer::PlanetViewer(StarSystem system, LoadProgress* progress)
        : m_system(std::move(system)),
          m_planetMap(
              m_system.planet,
              ClimateInputsFor(m_system),
              progress),
          m_regionMap(
              m_planetMap,
              [](const PlanetRegion& cell, float elevationMeters)
              {
                  // Tint by the local elevation, not the cell average, and skip the
                  // cell-wide lake wash: at this scale a lake is not the whole cell.
                  PlanetRegion local = cell;
                  local.elevationNormalized = std::clamp(
                      static_cast<double>(elevationMeters) / (kMaxElevationKm * 1000.0), -1.0, 1.0);
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
        const Vector3 lightDirection = Vector3Normalize(Vector3{ -0.45f, 0.62f, -0.85f });

        for (int latitude = 0; latitude < rows; ++latitude)
        {
            for (int longitude = 0; longitude < columns; ++longitude)
            {
                const PlanetRegion& region = m_planetMap.GetRegion(latitude, longitude);
                m_regionColors.push_back(GetRegionColor(region));
                const float latitudeRadians =
                    static_cast<float>(region.latitudeDegrees) * kPi / 180.0f;
                const float longitudeRadians =
                    static_cast<float>(region.longitudeDegrees) * kPi / 180.0f;
                const Vector3 normal{
                    std::cos(latitudeRadians) * std::cos(longitudeRadians),
                    std::sin(latitudeRadians),
                    -std::cos(latitudeRadians) * std::sin(longitudeRadians)
                };
                const int northLatitude = std::min(latitude + 1, rows - 1);
                const int southLatitude = std::max(latitude - 1, 0);
                const int eastLongitude = (longitude + 1) % columns;
                const int westLongitude = (longitude + columns - 1) % columns;
                const double northElevation =
                    m_planetMap.GetRegion(northLatitude, longitude)
                        .elevationNormalized * kMaxElevationKm;
                const double southElevation =
                    m_planetMap.GetRegion(southLatitude, longitude)
                        .elevationNormalized * kMaxElevationKm;
                const double eastElevation =
                    m_planetMap.GetRegion(latitude, eastLongitude)
                        .elevationNormalized * kMaxElevationKm;
                const double westElevation =
                    m_planetMap.GetRegion(latitude, westLongitude)
                        .elevationNormalized * kMaxElevationKm;
                const float reliefShade = std::clamp(
                    1.0f + static_cast<float>(
                        (southElevation - northElevation) * 0.12 +
                        (westElevation - eastElevation) * 0.12),
                    0.78f,
                    1.22f);
                const float sphericalShade =
                    0.24f + 0.76f * std::max(0.0f, Vector3DotProduct(normal, lightDirection));
                m_regionBrightness.push_back(sphericalShade * reliefShade);
            }
        }

        BeginLoadStep(progress, LoadStep::kViewerSmoothing, "Smoothing biome colors");
        const std::vector<Color> biomeColors = m_regionColors;
        constexpr int kColorOffsets[3] = { -1, 0, 1 };
        constexpr int kColorWeights[3][3] = {
            { 1, 1, 1 },
            { 1, 8, 1 },
            { 1, 1, 1 }
        };
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
                        const int neighborLatitude =
                            std::clamp(latitude + latitudeOffset, 0, rows - 1);
                        const int neighborLongitude =
                            (longitude + longitudeOffset + columns) % columns;
                        const std::size_t neighborIndex =
                            static_cast<std::size_t>(neighborLatitude) * columns +
                            neighborLongitude;
                        const std::size_t weight = static_cast<std::size_t>(
                            kColorWeights[latitudeOffset + 1][longitudeOffset + 1]);
                        const Color color = biomeColors[neighborIndex];
                        red += color.r * weight;
                        green += color.g * weight;
                        blue += color.b * weight;
                        totalWeight += weight;
                    }
                }
                const std::size_t index =
                    static_cast<std::size_t>(latitude) * columns + longitude;
                m_regionColors[index] = {
                    static_cast<unsigned char>(red / totalWeight),
                    static_cast<unsigned char>(green / totalWeight),
                    static_cast<unsigned char>(blue / totalWeight),
                    255
                };
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
                        const int neighborLatitude =
                            std::clamp(latitude + latitudeOffset, 0, rows - 1);
                        const int neighborLongitude =
                            (longitude + longitudeOffset + columns) % columns;
                        const int weight =
                            kColorWeights[latitudeOffset + 1][longitudeOffset + 1];
                        brightnessSum += cellBrightness[
                            static_cast<std::size_t>(neighborLatitude) * columns +
                            neighborLongitude] * weight;
                        weightSum += weight;
                    }
                }
                m_regionBrightness[
                    static_cast<std::size_t>(latitude) * columns + longitude] =
                    brightnessSum / static_cast<float>(weightSum);
            }
        }

        m_sphereVertices.reserve(
            static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
        m_sphereVertexColors.reserve(
            static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
        m_sphereVertexBrightness.reserve(
            static_cast<std::size_t>(rows + 1) * static_cast<std::size_t>(columns + 1));
        for (int latitude = 0; latitude <= rows; ++latitude)
        {
            for (int longitude = 0; longitude <= columns; ++longitude)
            {
                const int northCell = std::clamp(rows - 1 - latitude, 0, rows - 1);
                const int southCell = std::clamp(rows - latitude, 0, rows - 1);
                const int westCell = (longitude + columns - 1) % columns;
                const int eastCell = longitude % columns;
                const double meanElevationKm =
                    (m_planetMap.GetRegion(northCell, westCell).elevationNormalized +
                     m_planetMap.GetRegion(northCell, eastCell).elevationNormalized +
                     m_planetMap.GetRegion(southCell, westCell).elevationNormalized +
                     m_planetMap.GetRegion(southCell, eastCell).elevationNormalized) *
                    kMaxElevationKm / 4.0;
                const double planetRadiusMeters =
                    m_planetMap.GetParameters().radiusMeters;
                const float displayRadius = static_cast<float>(
                    1.0 + meanElevationKm * 1000.0 / planetRadiusMeters * 8.0);
                m_sphereVertices.push_back(GetSphereVertex(
                    latitude, longitude, rows, columns, displayRadius));
                const Color adjacentColors[4] = {
                    m_regionColors[static_cast<std::size_t>(northCell) * columns + westCell],
                    m_regionColors[static_cast<std::size_t>(northCell) * columns + eastCell],
                    m_regionColors[static_cast<std::size_t>(southCell) * columns + westCell],
                    m_regionColors[static_cast<std::size_t>(southCell) * columns + eastCell]
                };
                m_sphereVertexColors.push_back(AverageColors(adjacentColors, 4));
                const float adjacentBrightness[4] = {
                    m_regionBrightness[static_cast<std::size_t>(northCell) * columns + westCell],
                    m_regionBrightness[static_cast<std::size_t>(northCell) * columns + eastCell],
                    m_regionBrightness[static_cast<std::size_t>(southCell) * columns + westCell],
                    m_regionBrightness[static_cast<std::size_t>(southCell) * columns + eastCell]
                };
                m_sphereVertexBrightness.push_back(
                    (adjacentBrightness[0] + adjacentBrightness[1] +
                     adjacentBrightness[2] + adjacentBrightness[3]) * 0.25f);
            }
        }

    }

    void PlanetViewer::Update()
    {
        EnsureSurfaceTexture();
        m_regionMap.Update();
        HandleInput();

        const float frameTime = GetFrameTime();
        m_noticeSeconds = std::max(0.0f, m_noticeSeconds - frameTime);
        const float blendTarget = m_regionMode ? 1.0f : 0.0f;
        m_regionBlend += (blendTarget - m_regionBlend) * std::min(1.0f, frameTime * 4.0f);
        if (std::abs(blendTarget - m_regionBlend) < 0.002f)
        {
            m_regionBlend = blendTarget;
        }

        if (!m_draggingGlobe)
        {
            m_rotationDegrees = std::fmod(m_rotationDegrees + frameTime * 8.0f, 360.0f);
        }
    }

    void PlanetViewer::FocusRegionOnSelectedCell()
    {
        // A fresh chessboard anchored at the middle of the selected cell.
        m_regionMap.Anchor(PlanetaryCoordinates::ToCartesian(
            m_planetMap.GetPlanet().GetRegionCenter(m_selectedLatitude, m_selectedLongitude)));
    }

    std::optional<SettlementSite> PlanetViewer::TakeFoundedSite()
    {
        std::optional<SettlementSite> site;
        site.swap(m_foundedSite);
        return site;
    }

    void PlanetViewer::TryFoundSettlement()
    {
        const LocalGrid& grid = m_regionMap.GetGrid();
        const RegionId focus = m_regionMap.GetFocus();
        const double elevation =
            m_regionMap.GetTerrain().ElevationMeters(grid.CenterDirection(focus), SettlementTerrain::kMinWavelengthMeters);
        if (elevation < 0.0)
        {
            m_notice = "Cannot found a settlement at sea - pick a region on dry land.";
            m_noticeSeconds = 4.0f;
            return;
        }
        m_foundedSite = SettlementSite{ grid, focus };
    }

    void PlanetViewer::EnterRegionMode()
    {
        m_regionMode = true;
        FocusRegionOnSelectedCell();
    }

    void PlanetViewer::SyncSelectionToRegion()
    {
        const GeographicCoordinates center = m_regionMap.GetFocusCenter();
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();
        m_selectedLatitude = std::clamp(static_cast<int>((center.latitudeDegrees + 90.0) / 180.0 * rows), 0, rows - 1);
        m_selectedLongitude = std::clamp(static_cast<int>(center.longitudeDegrees / 360.0 * columns), 0, columns - 1);
    }

}
