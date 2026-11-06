#pragma once

#include <future>
#include <memory>
#include <optional>
#include <vector>

#include "domains/natural/planetary/world/WorldMap.h"
#include "presentation/ui/UniqueTexture.h"
#include "raylib.h"

namespace Infinitesia
{
// The whole 250 x 250 theater as one square image: biome colours, relief
// shading, lakes and rivers. This is the view that makes the theater window
// visible instead of showing only the patch around one tile.
class WorldMapView
{
public:
	WorldMapView() = default;
	~WorldMapView();

	WorldMapView(const WorldMapView &) = delete;
	WorldMapView &operator=(const WorldMapView &) = delete;

	// The world must outlive the view. Replacing the pointer rebuilds the image
	// on the next Update, which needs a GL context.
	void SetWorld(std::shared_ptr<const WorldMap> world);
	void Update();
	void Draw(Rectangle bounds, RegionId focus) const;
	bool IsLoading() const noexcept
	{
		return m_pendingPixels.valid() || m_queuedWorld != nullptr;
	}

	// Tile under the mouse, if the mouse is inside the drawn square.
	std::optional<RegionTileId> TileAt(Vector2 mouse, Rectangle bounds) const;

	static Rectangle FitSquare(Rectangle bounds) noexcept;

	void ReleaseRenderResources() noexcept;

private:
	static std::vector<Color> BuildPixels(const WorldMap &world);
	void StartBuild(std::shared_ptr<const WorldMap> world);
	void UploadPixels(const std::vector<Color> &pixels);

	std::shared_ptr<const WorldMap> m_world;
	std::shared_ptr<const WorldMap> m_pendingWorld;
	std::shared_ptr<const WorldMap> m_queuedWorld;
	std::future<std::vector<Color>> m_pendingPixels;
	const WorldMap *m_builtWorld = nullptr;
	std::vector<Color> m_pixels;
	UniqueTexture m_texture;
	bool m_dirty = false;
};
} // namespace Infinitesia
