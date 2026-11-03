#include "ui/PlanetViewer.h"

#include "coordinates/PlanetaryCoordinates.h"
#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace Infinitesia
{
    namespace
    {
        constexpr float kPanelPadding = 24.0f;
        constexpr float kHeaderHeight = 82.0f;
        constexpr float kFooterHeight = 92.0f;
        constexpr float kCameraFieldOfView = 45.0f;
        constexpr float kPi = 3.14159265358979323846f;

        struct ViewerLayout
        {
            Rectangle mapPanel{};
            Rectangle globePanel{};
            Rectangle mapBounds{};
            Vector2 globeCenter{};
            float globeRadius = 0.0f;
        };

        ViewerLayout GetLayout(int screenWidth, int screenHeight, const PlanetMap& planetMap)
        {
            const float width = static_cast<float>(screenWidth);
            const float height = static_cast<float>(screenHeight);
            const float contentTop = kHeaderHeight;
            const float contentBottom = std::max(contentTop + 1.0f, height - kFooterHeight);
            const float contentHeight = contentBottom - contentTop;
            const float splitX = width * 0.48f;

            ViewerLayout layout;
            layout.mapPanel = {
                kPanelPadding,
                contentTop,
                std::max(1.0f, splitX - kPanelPadding * 1.5f),
                contentHeight
            };
            layout.globePanel = {
                splitX,
                contentTop,
                std::max(1.0f, width - splitX - kPanelPadding),
                contentHeight
            };

            const float mapWidth = layout.mapPanel.width - 36.0f;
            const float mapHeight = layout.mapPanel.height - 100.0f;
            const float mapAspectRatio =
                static_cast<float>(planetMap.GetLongitudeSegmentCount()) /
                static_cast<float>(planetMap.GetLatitudeBandCount());
            float fittedWidth = std::min(mapWidth, mapHeight * mapAspectRatio);
            float fittedHeight = fittedWidth / mapAspectRatio;
            if (fittedWidth <= 0.0f || fittedHeight <= 0.0f)
            {
                fittedWidth = std::max(1.0f, mapWidth);
                fittedHeight = std::max(1.0f, mapHeight);
            }
            layout.mapBounds = {
                layout.mapPanel.x + (layout.mapPanel.width - fittedWidth) * 0.5f,
                layout.mapPanel.y + (layout.mapPanel.height - fittedHeight) * 0.5f,
                fittedWidth,
                fittedHeight
            };

            layout.globeCenter = {
                layout.globePanel.x + layout.globePanel.width * 0.5f,
                layout.globePanel.y + layout.globePanel.height * 0.5f
            };
            layout.globeRadius = std::max(
                1.0f,
                std::min(layout.globePanel.width * 0.39f, layout.globePanel.height * 0.42f));
            return layout;
        }

        float GetMinimumZoom(const ViewerLayout& layout, int screenHeight)
        {
            const float fieldOfViewRadians = kCameraFieldOfView * kPi / 180.0f;
            const float focalLength = static_cast<float>(screenHeight) /
                                      (2.0f * std::tan(fieldOfViewRadians * 0.5f));
            const float baseCameraDistance = std::max(
                2.4f,
                focalLength / layout.globeRadius * 1.08f);
            const float availableRadius =
                std::max(1.0f, std::min(layout.globePanel.width, layout.globePanel.height) * 0.5f - 12.0f);
            const float distanceScale =
                std::sqrt(1.0f + std::pow(focalLength / availableRadius, 2.0f)) /
                baseCameraDistance;
            return std::clamp(distanceScale, 0.55f, 2.0f);
        }

        Color LerpColor(Color first, Color second, float amount)
        {
            const float t = std::clamp(amount, 0.0f, 1.0f);
            return {
                static_cast<unsigned char>(first.r + (second.r - first.r) * t),
                static_cast<unsigned char>(first.g + (second.g - first.g) * t),
                static_cast<unsigned char>(first.b + (second.b - first.b) * t),
                255
            };
        }

        Color AverageColors(const Color* colors, std::size_t count)
        {
            std::size_t red = 0;
            std::size_t green = 0;
            std::size_t blue = 0;
            for (std::size_t index = 0; index < count; ++index)
            {
                red += colors[index].r;
                green += colors[index].g;
                blue += colors[index].b;
            }
            return {
                static_cast<unsigned char>(red / count),
                static_cast<unsigned char>(green / count),
                static_cast<unsigned char>(blue / count),
                255
            };
        }

        float SmootherStepF(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        Color GetBiomeColor(const PlanetRegion& region)
        {
            const float temp   = static_cast<float>(region.temperatureCelsius);
            const float precip = static_cast<float>(region.precipitationMm);
            // Physical elevation in km for shading calculations
            const float elevKm     = static_cast<float>(region.elevationNormalized * kMaxElevationKm);
            const float elevNorm   = static_cast<float>(region.elevationNormalized);
            // Land fraction: 0=sea level, 1=max mountain
            const float landElev   = std::clamp(elevNorm, 0.0f, 1.0f);

            const float stress = static_cast<float>(region.tectonicStress);

            switch (region.biome)
            {
                // ---- Aquatic ----
                case Biome::DeepOcean:
                    return LerpColor({ 10, 28, 70, 255 }, { 16, 45, 100, 255 },
                        std::clamp(-elevKm / static_cast<float>(kMaxElevationKm), 0.0f, 1.0f));

                case Biome::ShallowSea:
                {
                    const float depth = std::clamp(-elevKm / 5.0f, 0.0f, 1.0f);
                    return LerpColor({ 60, 185, 195, 255 }, { 20, 100, 155, 255 }, depth);
                }

                case Biome::CoralReef:
                    return LerpColor({ 64, 196, 168, 255 }, { 80, 210, 180, 255 },
                        SmootherStepF(static_cast<float>(region.humidityNormalized)));

                case Biome::PolarSea:
                    return LerpColor({ 140, 195, 210, 255 }, { 90, 155, 175, 255 },
                        std::clamp(-elevKm / 4.0f, 0.0f, 1.0f));

                // ---- Polar ice ----
                case Biome::IceCap:
                    return LerpColor({ 220, 235, 245, 255 }, { 245, 250, 255, 255 },
                        SmootherStepF(std::clamp((-temp - 20.0f) / 30.0f, 0.0f, 1.0f)));

                case Biome::Glacier:
                {
                    const float iciness = SmootherStepF(std::clamp((-temp) / 20.0f, 0.0f, 1.0f));
                    return LerpColor({ 165, 205, 220, 255 }, { 235, 245, 252, 255 }, iciness);
                }

                // ---- Tundra ----
                case Biome::Tundra:
                {
                    const float cold = SmootherStepF(std::clamp((-temp) / 12.0f, 0.0f, 1.0f));
                    return LerpColor({ 130, 148, 112, 255 }, { 185, 195, 180, 255 }, cold);
                }

                case Biome::WetTundra:
                {
                    const float wet = SmootherStepF(static_cast<float>(region.humidityNormalized));
                    return LerpColor({ 100, 130, 105, 255 }, { 80, 115, 95, 255 }, wet);
                }

                // ---- Boreal ----
                case Biome::BorealForest:
                {
                    const float dense = SmootherStepF(std::clamp(precip / 700.0f, 0.0f, 1.0f));
                    return LerpColor({ 55, 88, 62, 255 }, { 38, 68, 48, 255 }, dense);
                }

                case Biome::BorealWetland:
                    return LerpColor({ 62, 92, 78, 255 }, { 45, 75, 68, 255 },
                        SmootherStepF(static_cast<float>(region.soilMoisture)));

                // ---- Temperate ----
                case Biome::TemperateDeciduous:
                {
                    const float warmth = SmootherStepF(std::clamp((temp - 8.0f) / 12.0f, 0.0f, 1.0f));
                    const Color cool = { 68, 118, 72, 255 };
                    const Color warm = { 82, 140, 68, 255 };
                    const Color base = LerpColor(cool, warm, warmth);
                    // Altitude darkens slightly
                    return LerpColor(base, { 60, 96, 58, 255 }, SmootherStepF(landElev * 0.8f));
                }

                case Biome::TemperateMixed:
                {
                    const float moistBlend = SmootherStepF(std::clamp(precip / 600.0f, 0.0f, 1.0f));
                    return LerpColor({ 120, 148, 82, 255 }, { 78, 118, 72, 255 }, moistBlend);
                }

                case Biome::TemperateRainforest:
                    return LerpColor({ 46, 112, 78, 255 }, { 34, 86, 62, 255 },
                        SmootherStepF(std::clamp(precip / 2400.0f, 0.0f, 1.0f)));

                case Biome::TemperateSteppe:
                {
                    const float dryness = SmootherStepF(1.0f - std::clamp(precip / 450.0f, 0.0f, 1.0f));
                    return LerpColor({ 148, 162, 88, 255 }, { 172, 155, 100, 255 }, dryness);
                }

                case Biome::MediterraneanShrub:
                {
                    const float dry = SmootherStepF(static_cast<float>(region.drySeasonMonths) / 6.0f);
                    return LerpColor({ 148, 158, 92, 255 }, { 178, 148, 88, 255 }, dry);
                }

                // ---- Dry ----
                case Biome::ColdDesert:
                {
                    const float cold = SmootherStepF(std::clamp((-temp + 2.0f) / 14.0f, 0.0f, 1.0f));
                    return LerpColor({ 178, 165, 122, 255 }, { 148, 145, 120, 255 }, cold);
                }

                case Biome::HotDesert:
                {
                    const float heat = SmootherStepF(std::clamp((temp - 22.0f) / 22.0f, 0.0f, 1.0f));
                    return LerpColor({ 200, 175, 118, 255 }, { 210, 145, 88, 255 }, heat);
                }

                case Biome::SaltFlat:
                    return LerpColor({ 218, 212, 195, 255 }, { 235, 228, 215, 255 },
                        SmootherStepF(landElev));

                // ---- Tropical ----
                case Biome::Savanna:
                {
                    const float dryness = SmootherStepF(1.0f - std::clamp(precip / 1000.0f, 0.0f, 1.0f));
                    return LerpColor({ 148, 170, 82, 255 }, { 192, 162, 88, 255 }, dryness);
                }

                case Biome::TropicalDryForest:
                {
                    const float lush = SmootherStepF(std::clamp(precip / 1200.0f, 0.0f, 1.0f));
                    return LerpColor({ 108, 148, 72, 255 }, { 78, 128, 60, 255 }, lush);
                }

                case Biome::TropicalMonsoonForest:
                    return LerpColor({ 44, 128, 62, 255 }, { 30, 96, 46, 255 },
                        SmootherStepF(std::clamp(precip / 1800.0f, 0.0f, 1.0f)));

                case Biome::TropicalRainforest:
                {
                    const float lush = SmootherStepF(std::clamp(precip / 2000.0f, 0.0f, 1.0f));
                    return LerpColor({ 48, 110, 52, 255 }, { 28, 82, 38, 255 }, lush);
                }

                // ---- Alpine ----
                case Biome::AlpineGrassland:
                {
                    const float cold = SmootherStepF(std::clamp((-temp) / 8.0f, 0.0f, 1.0f));
                    return LerpColor({ 118, 142, 98, 255 }, { 148, 160, 130, 255 }, cold);
                }

                case Biome::AlpineRock:
                {
                    // Rock with snow patches at high elevation + low temp
                    const float snowBlend = SmootherStepF(std::clamp((elevKm - 3.0f) / 6.0f, 0.0f, 1.0f));
                    const float stressBright = 1.0f + stress * 0.25f;
                    const Color rock = {
                        static_cast<unsigned char>(std::clamp(92.0f * stressBright,  0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(88.0f * stressBright,  0.0f, 255.0f)),
                        static_cast<unsigned char>(std::clamp(84.0f * stressBright,  0.0f, 255.0f)),
                        255
                    };
                    return LerpColor(rock, { 225, 232, 238, 255 }, snowBlend);
                }

                // ---- Special ----
                case Biome::VolcanicTerrain:
                {
                    // Dark basalt with hot orange glow near active zones
                    const float hot = SmootherStepF(stress * 0.8f);
                    return LerpColor({ 55, 48, 42, 255 }, { 140, 72, 30, 255 }, hot);
                }
            }

            return { 128, 128, 128, 255 };
        }

        Color GetRegionColor(const PlanetRegion& region)
        {
            const Color biomeColor = GetBiomeColor(region);
            if (region.isOcean)
                return biomeColor;

            const float elevationKm =
                static_cast<float>(region.elevationNormalized * kMaxElevationKm);
            const float mountainBlend = SmootherStepF(
                std::clamp((elevationKm - 1.0f) / 4.0f, 0.0f, 1.0f));
            return LerpColor(
                biomeColor,
                { 132, 124, 112, 255 },
                mountainBlend * 0.55f);
        }

        Color ShadeColor(Color color, float brightness)
        {
            return {
                static_cast<unsigned char>(std::clamp(color.r * brightness, 0.0f, 255.0f)),
                static_cast<unsigned char>(std::clamp(color.g * brightness, 0.0f, 255.0f)),
                static_cast<unsigned char>(std::clamp(color.b * brightness, 0.0f, 255.0f)),
                color.a
            };
        }

        bool Contains(Rectangle rectangle, Vector2 point)
        {
            return point.x >= rectangle.x &&
                   point.x < rectangle.x + rectangle.width &&
                   point.y >= rectangle.y &&
                   point.y < rectangle.y + rectangle.height;
        }

        Vector3 GetSphereVertex(
            int row,
            int column,
            int rows,
            int columns,
            float radius)
        {
            const GeographicCoordinates coordinates{
                90.0 - static_cast<double>(row) / rows * 180.0,
                -static_cast<double>(column) / columns * 360.0
            };
            const CartesianPoint3D point = PlanetaryCoordinates::ToCartesian(coordinates);
            return {
                static_cast<float>(point.x) * radius,
                static_cast<float>(point.y) * radius,
                static_cast<float>(point.z) * radius
            };
        }

        constexpr Color kSelectedCellColor = { 255, 202, 87, 255 };
        constexpr int kColorBufferIndex = 3; // Mesh::vboId slot for vertex colors
    }

    PlanetViewer::PlanetViewer(StarSystem system)
        : m_system(std::move(system)),
          m_planetMap(
              m_system.planet,
              ClimateInputs{
                  m_system.planetOrbit.meanIncidentFluxInEarthUnits,
                  m_system.planet.axialTiltDegrees,
                  m_system.planet.atmospherePressureKPa,
                  m_system.planet.volatileMassFraction,
                  m_system.planet.gravityMetersPerSecondSquared,
                  m_system.planet.rotationHours,
                  m_system.planet.ageYears,
              }),
          m_selectedLatitude(m_planetMap.GetLatitudeBandCount() / 2),
          m_selectedLongitude(m_planetMap.GetLongitudeSegmentCount() / 2)
    {
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
        HandleInput();
        if (!m_draggingGlobe)
        {
            m_rotationDegrees = std::fmod(m_rotationDegrees + GetFrameTime() * 8.0f, 360.0f);
        }
    }

    void PlanetViewer::WriteCellColors(Color* out, int row, int column, bool selected) const
    {
        const int columns = m_planetMap.GetLongitudeSegmentCount();
        const std::size_t topLeft = static_cast<std::size_t>(row) * (columns + 1) + column;
        const std::size_t topRight = topLeft + 1;
        const std::size_t bottomLeft = topLeft + columns + 1;
        const std::size_t bottomRight = bottomLeft + 1;

        Color tl = kSelectedCellColor;
        Color tr = kSelectedCellColor;
        Color bl = kSelectedCellColor;
        Color br = kSelectedCellColor;
        if (!selected)
        {
            tl = ShadeColor(m_sphereVertexColors[topLeft], m_sphereVertexBrightness[topLeft]);
            tr = ShadeColor(m_sphereVertexColors[topRight], m_sphereVertexBrightness[topRight]);
            bl = ShadeColor(m_sphereVertexColors[bottomLeft], m_sphereVertexBrightness[bottomLeft]);
            br = ShadeColor(m_sphereVertexColors[bottomRight], m_sphereVertexBrightness[bottomRight]);
        }
        // Same triangle order as the former immediate-mode path.
        out[0] = tl; out[1] = bl; out[2] = tr;
        out[3] = tr; out[4] = bl; out[5] = br;
    }

    void PlanetViewer::SyncGlobeMesh() const
    {
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();
        const int selectedCell =
            (rows - 1 - m_selectedLatitude) * columns + m_selectedLongitude;

        if (m_globeMesh.vaoId == 0)
        {
            const int cellCount = rows * columns;
            Mesh mesh{};
            mesh.vertexCount = cellCount * 6;
            mesh.triangleCount = cellCount * 2;
            mesh.vertices = static_cast<float*>(
                RL_MALLOC(static_cast<std::size_t>(mesh.vertexCount) * 3 * sizeof(float)));
            mesh.colors = static_cast<unsigned char*>(
                RL_MALLOC(static_cast<std::size_t>(mesh.vertexCount) * sizeof(Color)));
            if (mesh.vertices == nullptr || mesh.colors == nullptr)
            {
                RL_FREE(mesh.vertices);
                RL_FREE(mesh.colors);
                throw std::runtime_error("Failed to allocate the globe mesh.");
            }

            auto* colors = reinterpret_cast<Color*>(mesh.colors);
            float* vertices = mesh.vertices;
            for (int row = 0; row < rows; ++row)
            {
                for (int column = 0; column < columns; ++column)
                {
                    const int cell = row * columns + column;
                    const std::size_t topLeft =
                        static_cast<std::size_t>(row) * (columns + 1) + column;
                    const Vector3 tl = m_sphereVertices[topLeft];
                    const Vector3 tr = m_sphereVertices[topLeft + 1];
                    const Vector3 bl = m_sphereVertices[topLeft + columns + 1];
                    const Vector3 br = m_sphereVertices[topLeft + columns + 2];
                    const Vector3 cellVertices[6] = { tl, bl, tr, tr, bl, br };
                    float* dst = vertices + static_cast<std::size_t>(cell) * 18;
                    for (const Vector3& vertex : cellVertices)
                    {
                        *dst++ = vertex.x;
                        *dst++ = vertex.y;
                        *dst++ = vertex.z;
                    }
                    WriteCellColors(
                        colors + static_cast<std::size_t>(cell) * 6,
                        row, column, cell == selectedCell);
                }
            }

            UploadMesh(&mesh, true);
            if (mesh.vaoId == 0)
            {
                RL_FREE(mesh.vertices);
                RL_FREE(mesh.colors);
                throw std::runtime_error("Failed to upload the globe mesh.");
            }
            m_globeMesh = mesh;
            m_globeMaterial = LoadMaterialDefault();
            m_uploadedSelectedCell = selectedCell;
            return;
        }

        if (selectedCell == m_uploadedSelectedCell)
            return;

        auto* colors = reinterpret_cast<Color*>(m_globeMesh.colors);
        const auto rewriteCell = [&](int cell, bool selected)
        {
            Color* cellColors = colors + static_cast<std::size_t>(cell) * 6;
            WriteCellColors(cellColors, cell / columns, cell % columns, selected);
            UpdateMeshBuffer(
                m_globeMesh,
                kColorBufferIndex,
                cellColors,
                static_cast<int>(6 * sizeof(Color)),
                static_cast<int>(static_cast<std::size_t>(cell) * 6 * sizeof(Color)));
        };
        if (m_uploadedSelectedCell >= 0)
            rewriteCell(m_uploadedSelectedCell, false);
        rewriteCell(selectedCell, true);
        m_uploadedSelectedCell = selectedCell;
    }

    void PlanetViewer::ReleaseRenderResources() noexcept
    {
        if (m_globeMesh.vaoId != 0)
        {
            UnloadMesh(m_globeMesh);
            UnloadMaterial(m_globeMaterial);
            m_globeMesh = {};
            m_globeMaterial = {};
            m_uploadedSelectedCell = -1;
        }
        if (m_surfaceTexture.id != 0)
        {
            UnloadTexture(m_surfaceTexture);
            m_surfaceTexture = {};
        }
    }

    Camera3D PlanetViewer::MakeCamera() const
    {
        const ViewerLayout layout = GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
        const float fieldOfViewRadians = kCameraFieldOfView * kPi / 180.0f;
        const float focalLength = static_cast<float>(GetScreenHeight()) /
                                  (2.0f * std::tan(fieldOfViewRadians * 0.5f));
        const float cameraDistance = std::max(
            2.4f,
            focalLength / layout.globeRadius * 1.08f) * m_zoom;
        const float targetZ =
            (static_cast<float>(GetScreenWidth()) * 0.5f - layout.globeCenter.x) *
            cameraDistance / focalLength;
        const float targetY =
            (layout.globeCenter.y - static_cast<float>(GetScreenHeight()) * 0.5f) *
            cameraDistance / focalLength;

        Camera3D camera{};
        camera.position = { -cameraDistance, targetY, targetZ };
        camera.target = { 0.0f, targetY, targetZ };
        camera.up = { 0.0f, 1.0f, 0.0f };
        camera.fovy = kCameraFieldOfView;
        camera.projection = CAMERA_PERSPECTIVE;
        return camera;
    }

    void PlanetViewer::HandleInput()
    {
        const Vector2 mousePosition = GetMousePosition();
        const ViewerLayout layout = GetLayout(GetScreenWidth(), GetScreenHeight(), m_planetMap);
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (Contains(layout.mapBounds, mousePosition))
            {
                const float normalizedX = (mousePosition.x - layout.mapBounds.x) / layout.mapBounds.width;
                const float normalizedY = (mousePosition.y - layout.mapBounds.y) / layout.mapBounds.height;
                m_selectedLongitude = std::clamp(
                    static_cast<int>(normalizedX * columns),
                    0,
                    columns - 1);
                m_selectedLatitude = rows - 1 - std::clamp(
                    static_cast<int>(normalizedY * rows),
                    0,
                    rows - 1);
                m_draggingGlobe = false;
            }
            else
            {
                const Camera3D camera = MakeCamera();
                const Ray ray = GetMouseRay(mousePosition, camera);
                const RayCollision collision = GetRayCollisionSphere(ray, { 0.0f, 0.0f, 0.0f }, 1.0f);
                m_draggingGlobe = collision.hit;
                m_lastMousePosition = mousePosition;

                if (collision.hit)
                {
                    const float yaw = -m_rotationDegrees * kPi / 180.0f;
                    const float pitch = -m_tiltDegrees * kPi / 180.0f;
                    const float cosYaw = std::cos(yaw);
                    const float sinYaw = std::sin(yaw);
                    const float xAfterYaw = cosYaw * collision.point.x + sinYaw * collision.point.z;
                    const float zAfterYaw = -sinYaw * collision.point.x + cosYaw * collision.point.z;
                    const float cosPitch = std::cos(pitch);
                    const float sinPitch = std::sin(pitch);
                    const float localY = cosPitch * collision.point.y - sinPitch * zAfterYaw;
                    const float localZ = sinPitch * collision.point.y + cosPitch * zAfterYaw;
                    const GeographicCoordinates coordinates = PlanetaryCoordinates::ToGeographic({
                        xAfterYaw,
                        localY,
                        -localZ
                    });
                    m_selectedLatitude = std::clamp(
                        static_cast<int>((coordinates.latitudeDegrees + 90.0) / 180.0 * rows),
                        0,
                        rows - 1);
                    m_selectedLongitude = std::clamp(
                        static_cast<int>(coordinates.longitudeDegrees / 360.0 * columns),
                        0,
                        columns - 1);
                }
            }
        }

        if (m_draggingGlobe && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            const Vector2 delta{
                mousePosition.x - m_lastMousePosition.x,
                mousePosition.y - m_lastMousePosition.y
            };
            m_rotationDegrees = std::fmod(m_rotationDegrees + delta.x * 0.35f, 360.0f);
            m_tiltDegrees = std::clamp(m_tiltDegrees + delta.y * 0.35f, -75.0f, 75.0f);
            m_lastMousePosition = mousePosition;
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            m_draggingGlobe = false;
        }

        m_zoom = std::clamp(
            m_zoom - GetMouseWheelMove() * 0.1f,
            GetMinimumZoom(layout, GetScreenHeight()),
            2.0f);
    }

    void PlanetViewer::Draw() const
    {
        const int screenWidth = GetScreenWidth();
        const int screenHeight = GetScreenHeight();
        const ViewerLayout layout = GetLayout(screenWidth, screenHeight, m_planetMap);
        const int rows = m_planetMap.GetLatitudeBandCount();
        const int columns = m_planetMap.GetLongitudeSegmentCount();

        if (m_surfaceTexture.id == 0)
        {
            Image surfaceImage = GenImageColor(columns, rows, BLACK);
            if (surfaceImage.data == nullptr)
                throw std::runtime_error("Failed to allocate the planet surface map image.");
            auto* surfacePixels = static_cast<Color*>(surfaceImage.data);
            for (int latitude = 0; latitude < rows; ++latitude)
            {
                for (int longitude = 0; longitude < columns; ++longitude)
                {
                    const std::size_t regionIndex =
                        static_cast<std::size_t>(latitude) * columns + longitude;
                    surfacePixels[
                        static_cast<std::size_t>(rows - 1 - latitude) * columns +
                        longitude] = m_regionColors[regionIndex];
                }
            }
            m_surfaceTexture = LoadTextureFromImage(surfaceImage);
            UnloadImage(surfaceImage);
            if (m_surfaceTexture.id == 0)
                throw std::runtime_error(
                    "Failed to create the planet surface map texture.");
            SetTextureFilter(m_surfaceTexture, TEXTURE_FILTER_BILINEAR);
        }

        ClearBackground({ 11, 17, 26, 255 });

        DrawText("INFINITESIA  /  PLANET VIEWER", 28, 22, 24, RAYWHITE);
        char systemSummary[192];
        std::snprintf(
            systemSummary,
            sizeof(systemSummary),
            "Star %.2f M_sun / %.2f L_sun  |  Age %.2f Gyr  |  Orbit %.2f AU  |  Planet %.2f M_earth  |  Seed %llu  |  Gen v%u",
            m_system.star.massKilograms / 1.98847e30,
            m_system.star.luminosityWatts / 3.828e26,
            m_system.galacticContext.systemAgeYears / 1.0e9,
            m_system.planetOrbit.semiMajorAxisMeters / 149597870700.0,
            m_system.planet.massKilograms / 5.9722e24,
            static_cast<unsigned long long>(m_system.scenario.seed),
            m_system.scenario.generatorVersion);
        DrawText(systemSummary, 30, 52, 15, { 139, 158, 177, 255 });
        DrawFPS(screenWidth - 80, 24);

        DrawRectangleRounded(layout.mapPanel, 0.035f, 8, { 19, 28, 40, 255 });
        DrawRectangleRounded(layout.globePanel, 0.035f, 8, { 19, 28, 40, 255 });
        DrawText("SURFACE MAP", static_cast<int>(layout.mapPanel.x + 18.0f), static_cast<int>(layout.mapPanel.y + 16.0f), 16, SKYBLUE);
        DrawText("ROTATING GLOBE", static_cast<int>(layout.globePanel.x + 18.0f), static_cast<int>(layout.globePanel.y + 16.0f), 16, SKYBLUE);

        const Rectangle textureSource{
            0.0f,
            0.0f,
            static_cast<float>(columns),
            static_cast<float>(rows)
        };
        DrawTexturePro(
            m_surfaceTexture,
            textureSource,
            layout.mapBounds,
            { 0.0f, 0.0f },
            0.0f,
            WHITE);
        const float cellWidth = layout.mapBounds.width / static_cast<float>(columns);
        const float cellHeight = layout.mapBounds.height / static_cast<float>(rows);
        const int selectedMapLatitude = rows - 1 - m_selectedLatitude;
        DrawRectangle(
            static_cast<int>(std::floor(
                layout.mapBounds.x + m_selectedLongitude * cellWidth)),
            static_cast<int>(std::floor(
                layout.mapBounds.y + selectedMapLatitude * cellHeight)),
            std::max(1, static_cast<int>(std::ceil(cellWidth))),
            std::max(1, static_cast<int>(std::ceil(cellHeight))),
            { 255, 202, 87, 255 });

        for (int longitude = 0; longitude <= 12; ++longitude)
        {
            const float x = layout.mapBounds.x + layout.mapBounds.width * longitude / 12.0f;
            DrawLine(
                static_cast<int>(x),
                static_cast<int>(layout.mapBounds.y),
                static_cast<int>(x),
                static_cast<int>(layout.mapBounds.y + layout.mapBounds.height),
                Fade(RAYWHITE, 0.12f));
        }
        for (int latitude = 0; latitude <= 6; ++latitude)
        {
            const float y = layout.mapBounds.y + layout.mapBounds.height * latitude / 6.0f;
            DrawLine(
                static_cast<int>(layout.mapBounds.x),
                static_cast<int>(y),
                static_cast<int>(layout.mapBounds.x + layout.mapBounds.width),
                static_cast<int>(y),
                Fade(RAYWHITE, 0.12f));
        }
        DrawRectangleLinesEx(layout.mapBounds, 1.0f, { 129, 158, 180, 255 });

        const Camera3D camera = MakeCamera();
        BeginScissorMode(
            static_cast<int>(layout.globePanel.x),
            static_cast<int>(layout.globePanel.y),
            static_cast<int>(layout.globePanel.width),
            static_cast<int>(layout.globePanel.height));
        BeginMode3D(camera);
        SyncGlobeMesh();
        const Matrix globeTransform = MatrixMultiply(
            MatrixRotateX(m_tiltDegrees * DEG2RAD),
            MatrixRotateY(m_rotationDegrees * DEG2RAD));
        DrawMesh(m_globeMesh, m_globeMaterial, globeTransform);
        EndMode3D();
        EndScissorMode();

        char selectionText[256];
        const PlanetRegion& selectedRegion =
            m_planetMap.GetRegion(m_selectedLatitude, m_selectedLongitude);

        const char* biomeName = [](Biome b) -> const char* {
            switch (b) {
                case Biome::DeepOcean:           return "Deep Ocean";
                case Biome::ShallowSea:          return "Shallow Sea";
                case Biome::CoralReef:           return "Coral Reef";
                case Biome::PolarSea:            return "Polar Sea";
                case Biome::IceCap:              return "Ice Cap";
                case Biome::Glacier:             return "Glacier";
                case Biome::Tundra:              return "Tundra";
                case Biome::WetTundra:           return "Wet Tundra";
                case Biome::BorealForest:        return "Boreal Forest";
                case Biome::BorealWetland:       return "Boreal Wetland";
                case Biome::TemperateDeciduous:  return "Temperate Deciduous";
                case Biome::TemperateMixed:      return "Temperate Mixed";
                case Biome::TemperateRainforest: return "Temperate Rainforest";
                case Biome::TemperateSteppe:     return "Temperate Steppe";
                case Biome::MediterraneanShrub:  return "Mediterranean Shrub";
                case Biome::ColdDesert:          return "Cold Desert";
                case Biome::HotDesert:           return "Hot Desert";
                case Biome::SaltFlat:            return "Salt Flat";
                case Biome::Savanna:             return "Savanna";
                case Biome::TropicalDryForest:   return "Tropical Dry Forest";
                case Biome::TropicalMonsoonForest: return "Tropical Monsoon Forest";
                case Biome::TropicalRainforest:  return "Tropical Rainforest";
                case Biome::AlpineGrassland:     return "Alpine Grassland";
                case Biome::AlpineRock:          return "Alpine Rock";
                case Biome::VolcanicTerrain:     return "Volcanic Terrain";
                default:                         return "Unknown";
            }
        }(selectedRegion.biome);

        std::snprintf(
            selectionText,
            sizeof(selectionText),
            "Region %d, %d  |  %.1f deg, %.1f deg  |  %.2f km  |  %.1f C  |  %.0f mm/yr  |  %s",
            m_selectedLatitude,
            m_selectedLongitude,
            selectedRegion.latitudeDegrees,
            selectedRegion.longitudeDegrees,
            selectedRegion.elevationNormalized * kMaxElevationKm,   // physical km
            selectedRegion.temperatureCelsius,
            selectedRegion.precipitationMm,
            biomeName);

        const int footerY = screenHeight - 70;
        DrawText(selectionText, 30, footerY, 16, RAYWHITE);

        // Second footer line: geological/hydrological details
        const char* rockName = [](RockType r) -> const char* {
            switch (r) {
                case RockType::BasalticOceanic:   return "Basaltic";
                case RockType::GraniticShield:    return "Granitic Shield";
                case RockType::SedimentaryBasin:  return "Sedimentary";
                case RockType::VolcanicIgneous:   return "Volcanic";
                case RockType::MetamorphicBelt:   return "Metamorphic";
                case RockType::FoldedSedimentary: return "Folded Sedimentary";
                default: return "Unknown";
            }
        }(selectedRegion.surfaceRock);

        char detailText[384];
        if (selectedRegion.isLake)
        {
            std::snprintf(
                detailText, sizeof(detailText),
                "Lake V %.3f km3  |  Annual in/evap/infil/spill %.3f/%.3f/%.3f/%.3f km3/yr"
                "  |  Level %.2f km  |  Spill height %.3f km",
                selectedRegion.lakeWaterVolumeKm3,
                selectedRegion.lakeAnnualInflowKm3,
                selectedRegion.lakeAnnualEvaporationKm3,
                selectedRegion.lakeAnnualInfiltrationKm3,
                selectedRegion.lakeAnnualSpillKm3,
                selectedRegion.lakeWaterLevelKm,
                selectedRegion.lakeSpillHeightKm);
        }
        else
        {
            std::snprintf(
                detailText, sizeof(detailText),
                "%s  |  %s  |  Stress %.2f  |  %s  |  Soil %.0f%%  |  River: %s",
                rockName,
                selectedRegion.isVolcanicZone ? "Volcanic" : "Stable",
                selectedRegion.tectonicStress,
                selectedRegion.isOcean ? "Ocean" : "Land",
                selectedRegion.soilFertility * 100.0,
                selectedRegion.hasRiver ? "Yes" : "No");
        }
        DrawText(detailText, 30, footerY + 20, 14, { 180, 195, 210, 255 });

        char seasonalText[256];
        constexpr int kSeasonMonths[4] = { 0, 3, 6, 9 };
        std::snprintf(
            seasonalText,
            sizeof(seasonalText),
            "Jan %.0f C / %.0f mm  |  Apr %.0f C / %.0f mm  |  Jul %.0f C / %.0f mm  |  Oct %.0f C / %.0f mm",
            selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[0]],
            selectedRegion.monthlyPrecipitationMm[kSeasonMonths[0]],
            selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[1]],
            selectedRegion.monthlyPrecipitationMm[kSeasonMonths[1]],
            selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[2]],
            selectedRegion.monthlyPrecipitationMm[kSeasonMonths[2]],
            selectedRegion.monthlyTemperatureCelsius[kSeasonMonths[3]],
            selectedRegion.monthlyPrecipitationMm[kSeasonMonths[3]]);
        DrawText(
            seasonalText,
            30,
            footerY + 40,
            13,
            { 100, 118, 140, 255 });
        DrawText(
            "Click the map or globe to select a region  |  Drag to rotate  |  Scroll to zoom",
            30,
            footerY + 57,
            12,
            { 100, 118, 140, 255 });
    }
}
