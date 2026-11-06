#pragma once

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <vector>

#include "domains/natural/planetary/PlanetMap.h"
#include "domains/natural/planetary/region/RegionGenerator.h"
#include "presentation/ui/UniqueTexture.h"
#include "raylib.h"

namespace Infinitesia
{
// Top-down map of the area around one region: hillshaded relief tinted by
// biome. The patch is built on a worker thread; the previous image stays up
// until the next one is ready.
class RegionMapView
{
public:
	// Tint for a point of the planet cell it falls in, at a local elevation.
	using ColorFunction = std::function<Color(const PlanetRegion &, float elevationMeters)>;

	// Regions on each side of the focus that the map covers. 8 => 17 x 17
	// theater tiles, about 17 km across. The image is coarsened for large radii.
	static constexpr int kDefaultPatchRadiusRegions = 8;

	RegionMapView(const PlanetMap &map, ColorFunction color);
	~RegionMapView();

	RegionMapView(const RegionMapView &) = delete;
	RegionMapView &operator=(const RegionMapView &) = delete;

	const LocalGrid &GetGrid() const noexcept
	{
		return m_grid;
	}
	const WorldTerrain &GetTerrain() const noexcept
	{
		return m_generator.GetTerrain();
	}
	std::shared_ptr<const WorldMap> GetWorldMap() const noexcept
	{
		return m_worldMap;
	}

	// Lays a new chessboard anchored at a point of the planet and focuses its
	// middle region.
	void Anchor(CartesianPoint3D direction);
	// Focus a region of the current grid, by id or by the point it contains.
	void Focus(RegionId id);
	void FocusAt(CartesianPoint3D direction);
	RegionId GetFocus() const noexcept
	{
		return m_focus;
	}
	int PatchRadius() const noexcept
	{
		return m_patchRadius;
	}
	double PatchSpanMeters() const noexcept;
	GeographicCoordinates GetFocusCenter() const noexcept;
	// The biome shown at the focused tile, if a patch has been built.
	std::optional<Biome> FocusBiome() const;
	// Change the preview scale (the wheel in region mode).
	void Zoom(int steps);
	bool IsLoading() const noexcept
	{
		return m_pending.valid();
	}

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

	static Built Build(const RegionGenerator &generator, const ColorFunction &color, const LocalGrid &grid,
	                   RegionId focus, int radiusRegions);
	bool PatchPoint(Vector2 mouse, Rectangle bounds, int &x, int &y) const;

	std::shared_ptr<const WorldMap> m_worldMap;
	RegionGenerator m_generator;
	ColorFunction m_color;

	LocalGrid m_grid;
	unsigned m_gridVersion = 0; // bumped whenever the anchor moves
	RegionId m_focus{};         // requested
	RegionId m_displayed{};     // what the texture and patch show
	int m_patchRadius = kDefaultPatchRadiusRegions;
	int m_displayedRadius = 0;
	int m_pendingRadius = 0;
	unsigned m_displayedVersion = 0;
	unsigned m_pendingVersion = 0;
	bool m_hasImage = false;
	std::future<Built> m_pending;
	RegionId m_pendingFocus{};
	std::shared_ptr<const RegionPatch> m_patch;
	UniqueTexture m_texture;
};
} // namespace Infinitesia
