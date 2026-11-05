#include "presentation/ui/WorldMapView.h"

#include <algorithm>
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

	// A river is a whole region tile on this map, so a blue wash is enough to
	// read the network at 768 m per pixel.
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

WorldMapView::~WorldMapView() = default;

void WorldMapView::SetWorld(std::shared_ptr<const WorldMap> world)
{
	if (m_world.get() == world.get())
	{
		return;
	}
	m_world = std::move(world);
	m_dirty = true;
}

void WorldMapView::ReleaseRenderResources() noexcept
{
	m_texture.Reset();
	m_builtWorld = nullptr;
	m_dirty = true;
}

void WorldMapView::BuildTexture()
{
	if (!m_world)
	{
		return;
	}

	constexpr int side = kRegionTilesPerWorldSide;
	constexpr int half = kRegionTilesPerWorldHalfSide;
	m_pixels.assign(static_cast<std::size_t>(side) * static_cast<std::size_t>(side), BLACK);
	for (int j = -half; j <= half; ++j)
	{
		for (int i = -half; i <= half; ++i)
		{
			const std::size_t imageIndex = static_cast<std::size_t>(half - j) * static_cast<std::size_t>(side) +
			                               static_cast<std::size_t>(i + half);
			m_pixels[imageIndex] = TileWithRelief(*m_world, {i, j});
		}
	}

	const Image image{m_pixels.data(), side, side, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
	Texture2D texture = LoadTextureFromImage(image);
	if (texture.id != 0)
	{
		SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
		m_texture = UniqueTexture(texture);
	}
	m_builtWorld = m_world.get();
}

void WorldMapView::Update()
{
	if (m_dirty && m_world && m_builtWorld != m_world.get())
	{
		BuildTexture();
		m_dirty = false;
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
	if (!m_texture.IsValid())
	{
		DrawRectangleRec(square, {14, 21, 31, 255});
		DrawText("Generating world map...", static_cast<int>(square.x + 16.0f), static_cast<int>(square.y + 16.0f), 18,
		         {139, 158, 177, 255});
		return;
	}

	DrawTexturePro(
	    m_texture.Get(),
	    {0.0f, 0.0f, static_cast<float>(kRegionTilesPerWorldSide), static_cast<float>(kRegionTilesPerWorldSide)},
	    square, {0.0f, 0.0f}, 0.0f, WHITE);

	// Sparse grid every 32 region tiles, not each of the 257 lines.
	constexpr int kGridStep = 32;
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
		const float x = square.x + static_cast<float>(focus.i + kRegionTilesPerWorldHalfSide) * cell;
		const float y = square.y + static_cast<float>(kRegionTilesPerWorldHalfSide - focus.j) * cell;
		DrawRectangleLinesEx({x, y, std::max(2.0f, cell), std::max(2.0f, cell)}, 2.0f, kSelectedCellColor);
	}

	DrawRectangleLinesEx(square, 1.0f, {129, 158, 180, 255});
}
} // namespace Infinitesia
