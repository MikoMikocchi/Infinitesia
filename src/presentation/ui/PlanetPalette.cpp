#include "presentation/ui/PlanetPalette.h"

#include <algorithm>
#include <cmath>

namespace Infinitesia
{
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

    namespace
    {
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
    }

    Color GetRegionColor(const PlanetRegion& region)
    {
        const Color biomeColor = GetBiomeColor(region);
        if (region.isOcean)
            return biomeColor;

        // A lake covers only part of its cell: tint the land colour toward
        // water by the covered fraction. sqrt keeps small lakes visible.
        const auto withLake = [&region](Color land)
        {
            if (!region.isLake)
                return land;
            const float depth = std::clamp(
                static_cast<float>(region.lakeSpillHeightKm) / 0.6f, 0.0f, 1.0f);
            const Color water = LerpColor({ 70, 150, 190, 255 }, { 24, 84, 140, 255 }, depth);
            return LerpColor(
                land, water,
                std::sqrt(static_cast<float>(region.lakeFraction)));
        };

        const float elevationKm =
            static_cast<float>(region.elevationNormalized * kMaxElevationKm);
        const float mountainBlend = SmootherStepF(
            std::clamp((elevationKm - 1.0f) / 4.0f, 0.0f, 1.0f));
        return withLake(LerpColor(
            biomeColor,
            { 132, 124, 112, 255 },
            mountainBlend * 0.55f));
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

    const char* BiomeName(Biome biome)
    {
        switch (biome)
        {
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
    }
}
