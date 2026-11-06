#include "presentation/ui/WorldMapView.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "presentation/ui/PlanetPalette.h"

namespace Infinitesia
{
namespace
{
Color TileColor(const WorldMap &world, RegionTileId id)
{
	const RegionTile &tile = world.At(id);
	PlanetRegion region;
	region.elevationNormalized =
	    std::clamp(static_cast<double>(tile.elevationMeters) / (kMaxElevationKm * 1000.0), -1.0, 1.0);
	region.temperatureCelsius = tile.temperatureCelsius;
	region.precipitationMm = tile.precipitationMm;
	region.drySeasonMonths = tile.drySeasonMonths;
	region.soilFertility = tile.soilFertility;
	region.soilMoisture = std::clamp(static_cast<double>(tile.precipitationMm) / 1200.0, 0.0, 1.0);
	region.humidityNormalized = std::clamp(static_cast<double>(tile.precipitationMm) / 1500.0, 0.0, 1.0);
	region.biome = tile.biome;
	region.isOcean = tile.IsOcean();
	region.isLake = tile.IsLake();
	region.lakeFraction = tile.IsLake() ? 1.0 : 0.0;
	region.lakeSpillHeightKm = tile.IsLake() ? static_cast<double>(tile.waterDepthMeters) / 1000.0 : 0.0;

	Color color = GetRegionColor(region);

	// A river is a whole theater tile on this map, so a blue wash is enough to
	// read the network at 1024 m per pixel.
	if (tile.HasRiver() && tile.riverCount > 0 && !tile.IsOcean() && !tile.IsLake())
	{
		color = LerpColor(color, {48, 128, 196, 255}, 0.72f);
	}
	return color;
}

Color TileWithRelief(const WorldMap &world, RegionTileId id)
{
	Color color = TileColor(world, id);
	const auto elevationAt = [&world, id](int i, int j) {
		return world.Contains({i, j}) ? world.At(i, j).elevationMeters : world.At(id).elevationMeters;
	};
	const float west = elevationAt(id.i - 1, id.j);
	const float east = elevationAt(id.i + 1, id.j);
	const float north = elevationAt(id.i, id.j + 1);
	const float south = elevationAt(id.i, id.j - 1);
	const float brightness = std::clamp(1.0f + (west - east) * 0.0015f + (south - north) * 0.0015f, 0.72f, 1.28f);
	return ShadeColor(color, brightness);
}
} // namespace

WorldMapView::~WorldMapView()
{
	if (m_pendingPixels.valid())
	{
		m_pendingPixels.wait();
	}
}

void WorldMapView::StartBuild(std::shared_ptr<const WorldMap> world)
{
	m_pendingWorld = std::move(world);
	m_pendingPixels = std::async(std::launch::async, [world = m_pendingWorld] {
		return BuildPixels(*world);
	});
}

void WorldMapView::SetWorld(std::shared_ptr<const WorldMap> world)
{
	if (m_world.get() == world.get())
	{
		return;
	}
	m_world = std::move(world);
	if (!m_world)
	{
		m_texture.Reset();
		m_builtWorld = nullptr;
		return;
	}
	if (m_pendingPixels.valid())
	{
		// A newer theater arrived while the previous image is still being built;
		// let the old worker finish and start this one next.
		m_queuedWorld = m_world;
		return;
	}
	StartBuild(m_world);
}

void WorldMapView::ReleaseRenderResources() noexcept
{
	if (m_pendingPixels.valid())
	{
		m_pendingPixels.wait();
		m_pendingPixels = {};
	}
	m_pendingWorld.reset();
	m_queuedWorld.reset();
	m_texture.Reset();
	m_world.reset();
	m_builtWorld = nullptr;
	m_dirty = true;
}

std::vector<Color> WorldMapView::BuildPixels(const WorldMap &world)
{
	constexpr int side = kRegionTilesPerWorldSide;
	constexpr int half = kRegionTilesPerWorldHalfSide;
	std::vector<Color> pixels(static_cast<std::size_t>(side) * static_cast<std::size_t>(side), BLACK);
	// The image is north-up. Tile j = half - 1 is the north edge, so image row
	// zero is (half - 1 - j).
	for (int j = -half; j < half; ++j)
	{
		for (int i = -half; i < half; ++i)
		{
			const std::size_t imageIndex = static_cast<std::size_t>(half - 1 - j) * static_cast<std::size_t>(side) +
			                               static_cast<std::size_t>(i + half);
			pixels[imageIndex] = TileWithRelief(world, {i, j});
		}
	}
	return pixels;
}

void WorldMapView::UploadPixels(const std::vector<Color> &pixels)
{
	constexpr int side = kRegionTilesPerWorldSide;
	const Image image{const_cast<Color *>(pixels.data()), side, side, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
	Texture2D texture = LoadTextureFromImage(image);
	if (texture.id != 0)
	{
		SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
		m_texture = UniqueTexture(texture);
	}
	m_builtWorld = m_pendingWorld.get();
}

void WorldMapView::Update()
{
	if (m_pendingPixels.valid() &&
	    m_pendingPixels.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		std::vector<Color> pixels = m_pendingPixels.get();
		if (m_queuedWorld)
		{
			// Drop the stale image; the queued theater is the one the player
			// asked for last.
			std::shared_ptr<const WorldMap> next = std::move(m_queuedWorld);
			m_queuedWorld.reset();
			StartBuild(std::move(next));
		}
		else
		{
			m_pixels = std::move(pixels);
			UploadPixels(m_pixels);
			m_pendingWorld.reset();
			m_dirty = false;
		}
	}
}

Rectangle WorldMapView::FitSquare(Rectangle bounds) noexcept
{
	const float side = std::max(1.0f, std::min(bounds.width, bounds.height));
	return {bounds.x + (bounds.width - side) * 0.5f, bounds.y + (bounds.height - side) * 0.5f, side, side};
}

std::optional<RegionTileId> WorldMapView::TileAt(Vector2 mouse, Rectangle bounds) const
{
	const Rectangle square = FitSquare(bounds);
	if (mouse.x < square.x || mouse.x >= square.x + square.width || mouse.y < square.y ||
	    mouse.y >= square.y + square.height)
	{
		return std::nullopt;
	}
	const float normalizedX = (mouse.x - square.x) / square.width;
	const float normalizedY = 1.0f - (mouse.y - square.y) / square.height;
	const int i = std::clamp(static_cast<int>(std::floor(normalizedX * kRegionTilesPerWorldSide)), 0,
	                         kRegionTilesPerWorldSide - 1) -
	              kRegionTilesPerWorldHalfSide;
	const int j = std::clamp(static_cast<int>(std::floor(normalizedY * kRegionTilesPerWorldSide)), 0,
	                         kRegionTilesPerWorldSide - 1) -
	              kRegionTilesPerWorldHalfSide;
	const RegionTileId id{i, j};
	return WorldMap::Contains(id) ? std::optional<RegionTileId>(id) : std::nullopt;
}

void WorldMapView::Draw(Rectangle bounds, RegionId focus) const
{
	const Rectangle square = FitSquare(bounds);
	if (!m_texture.IsValid() || m_pendingPixels.valid() || m_queuedWorld)
	{
		DrawRectangleRec(square, {14, 21, 31, 255});
		DrawText("Generating theater map...", static_cast<int>(square.x + 16.0f), static_cast<int>(square.y + 16.0f), 18,
		         {139, 158, 177, 255});
		return;
	}

	DrawTexturePro(
	    m_texture.Get(),
	    {0.0f, 0.0f, static_cast<float>(kRegionTilesPerWorldSide), static_cast<float>(kRegionTilesPerWorldSide)},
	    square, {0.0f, 0.0f}, 0.0f, WHITE);

	// Sparse grid every 25 theater tiles, not each of the 250 lines.
	constexpr int kGridStep = 25;
	const float cell = square.width / static_cast<float>(kRegionTilesPerWorldSide);
	for (int step = 0; step <= kRegionTilesPerWorldSide; step += kGridStep)
	{
		const float offset = square.x + static_cast<float>(step) * cell;
		DrawLine(static_cast<int>(offset), static_cast<int>(square.y), static_cast<int>(offset),
		         static_cast<int>(square.y + square.height), Fade(RAYWHITE, 0.10f));
		const float vertical = square.y + static_cast<float>(step) * cell;
		DrawLine(static_cast<int>(square.x), static_cast<int>(vertical), static_cast<int>(square.x + square.width),
		         static_cast<int>(vertical), Fade(RAYWHITE, 0.10f));
	}

	if (WorldMap::Contains({focus.i, focus.j}))
	{
		const float tileX = square.x + static_cast<float>(focus.i + kRegionTilesPerWorldHalfSide) * cell;
		const float tileY = square.y + static_cast<float>(kRegionTilesPerWorldHalfSide - 1 - focus.j) * cell;
		// A tile is only a few pixels at the full theater scale. Draw a pulsing
		// reticle that is always large enough to see.
		const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 4.0f);
		const float marker = std::max(18.0f, cell * 3.5f) + pulse * 2.0f;
		const float markerX =
		    std::clamp(tileX + cell * 0.5f - marker * 0.5f, square.x, square.x + square.width - marker);
		const float markerY =
		    std::clamp(tileY + cell * 0.5f - marker * 0.5f, square.y, square.y + square.height - marker);
		const Rectangle reticle{markerX, markerY, marker, marker};
		DrawRectangleRec(reticle, Fade(kSelectedCellColor, 0.10f + 0.08f * pulse));
		DrawRectangleLinesEx(reticle, 2.0f, kSelectedCellColor);
		const float centreX = reticle.x + reticle.width * 0.5f;
		const float centreY = reticle.y + reticle.height * 0.5f;
		DrawLine(static_cast<int>(reticle.x), static_cast<int>(centreY),
		         static_cast<int>(reticle.x + reticle.width), static_cast<int>(centreY), kSelectedCellColor);
		DrawLine(static_cast<int>(centreX), static_cast<int>(reticle.y), static_cast<int>(centreX),
		         static_cast<int>(reticle.y + reticle.height), kSelectedCellColor);
	}

	DrawRectangleLinesEx(square, 1.0f, {129, 158, 180, 255});
}
} // namespace Infinitesia
