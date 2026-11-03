#pragma once

#include "planet/PlanetMap.h"
#include "planet/region/RegionGenerator.h"
#include "raylib.h"
#include "ui/UniqueTexture.h"

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace Infinitesia
{
    // Top-down map of the area around one region: hillshaded relief tinted by
    // biome. The patch is built on a worker thread; the previous image stays up
    // until the next one is ready.
    class RegionMapView
    {
    public:
        // Tint for a point of the planet cell it falls in, at a local elevation.
        using ColorFunction = std::function<Color(const PlanetRegion&, float elevationMeters)>;

        // Regions on each side of the focus that the map covers (1 => 3 x 3 regions, ~12 km).
        static constexpr int kPatchRadiusRegions = 1;

        RegionMapView(const PlanetMap& map, ColorFunction color);
        ~RegionMapView();

        RegionMapView(const RegionMapView&) = delete;
        RegionMapView& operator=(const RegionMapView&) = delete;

        const RegionLayout& GetLayout() const noexcept { return m_generator.GetLayout(); }
        const RegionTerrain& GetTerrain() const noexcept { return m_generator.GetTerrain(); }

        void Focus(RegionId id);
        void FocusAt(CartesianPoint3D direction);
        RegionId GetFocus() const noexcept { return m_focus; }
        GeographicCoordinates GetFocusCenter() const noexcept;
        bool IsLoading() const noexcept { return m_pending.valid(); }

        // Polls the worker and uploads finished images; needs the GL context.
        void Update();
        void Draw(Rectangle bounds) const;

        // Direction under the mouse, if the map is shown and the mouse is on it.
        std::optional<CartesianPoint3D> DirectionAt(Vector2 mouse, Rectangle bounds) const;
        std::optional<float> ElevationAt(Vector2 mouse, Rectangle bounds) const;
        std::optional<Biome> BiomeAt(Vector2 mouse, Rectangle bounds) const;

        // Square image area inside bounds.
        static Rectangle FitSquare(Rectangle bounds) noexcept;

        void ReleaseRenderResources() noexcept;

    private:
        struct Built
        {
            RegionPatch patch;
            std::vector<Color> pixels;
        };

        static Built Build(const RegionGenerator& generator, const PlanetMap& map,
                           const ColorFunction& color, RegionId focus);
        bool PatchPoint(Vector2 mouse, Rectangle bounds, int& x, int& y) const;

        const PlanetMap& m_map;
        RegionGenerator m_generator;
        ColorFunction m_color;

        RegionId m_focus{};            // requested
        RegionId m_displayed{};        // what the texture and patch show
        bool m_hasImage = false;
        std::future<Built> m_pending;
        RegionId m_pendingFocus{};
        std::shared_ptr<const RegionPatch> m_patch;
        UniqueTexture m_texture;
    };
}
