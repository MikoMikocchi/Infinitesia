#include "presentation/ui/RegionMapView.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

#include "core/PlanetaryCoordinates.h"

namespace Infinitesia
{
namespace
{
constexpr float kReliefExaggeration = 3.0f;
constexpr Color kFocusColor = {255, 202, 87, 255};

Color Lerp(Color first, Color second, float amount)
{
	const float t = std::clamp(amount, 0.0f, 1.0f);
	return {static_cast<unsigned char>(first.r + (second.r - first.r) * t),
	        static_cast<unsigned char>(first.g + (second.g - first.g) * t),
	        static_cast<unsigned char>(first.b + (second.b - first.b) * t), 255};
}

bool IsAquatic(Biome biome)
{
	return biome == Biome::DeepOcean || biome == Biome::ShallowSea || biome == Biome::CoralReef ||
	       biome == Biome::PolarSea;
}

double CellSizeForRadius(int radiusRegions) noexcept
{
	if (radiusRegions <= 2)
	{
		return 16.0;
	}
	if (radiusRegions <= 4)
	{
		return 32.0;
	}
	return 64.0;
}
} // namespace

RegionMapView::RegionMapView(const PlanetMap &map, TheaterCache &theaters, ColorFunction color)
    : m_theaters(theaters), m_generator(map), m_color(std::move(color))
{
	m_grid = m_generator.MakeGrid(PlanetaryCoordinates::ToCartesian({0.0, 0.0}));
}

RegionMapView::~RegionMapView()
{
	// The patch worker reads m_generator's planet and the world it captured;
	// wait for it before members go away.
	if (m_pending.valid())
	{
		m_pending.wait();
	}
}

void RegionMapView::RequestTheater(const TheaterKey &key, std::optional<CartesianPoint3D> focus)
{
	m_requestedTheater = key;
	m_requestedFocus = focus;

	// Already showing this theater: just move the focus, no regeneration and no
	// new grid.
	if (m_displayedTheater.has_value() && *m_displayedTheater == key && m_worldMap != nullptr)
	{
		if (focus.has_value())
		{
			const RegionId id = m_grid.RegionAt(*focus);
			if (LocalGrid::IsValid(id))
			{
				m_focus = id;
			}
		}
		m_requestedFocus.reset();
		return;
	}

	// Trigger a worker now; Update takes the map when it is ready. Until then
	// the old theater and image stay in place.
	m_theaters.GetOrCreate(key);
}

void RegionMapView::RequestTheaterAt(CartesianPoint3D direction)
{
	RequestTheater(m_theaters.KeyAt(direction), direction);
}

void RegionMapView::ApplyTheater(const TheaterKey &key, std::shared_ptr<const WorldMap> world)
{
	m_worldMap = std::move(world);
	m_displayedTheater = key;
	m_generator.SetWorld(m_worldMap);
	m_grid = m_generator.MakeGrid(PlanetaryCoordinates::ToCartesian(m_theaters.Anchor(key)));
	++m_gridVersion;
	m_focus = RegionId{};
	if (m_requestedFocus.has_value())
	{
		const RegionId id = m_grid.RegionAt(*m_requestedFocus);
		if (LocalGrid::IsValid(id))
		{
			m_focus = id;
		}
	}
	m_requestedFocus.reset();
	m_hasImage = false;
}

void RegionMapView::Focus(RegionId id)
{
	if (LocalGrid::IsValid(id))
	{
		m_focus = id;
	}
}

void RegionMapView::FocusAt(CartesianPoint3D direction)
{
	Focus(m_grid.RegionAt(direction));
}

GeographicCoordinates RegionMapView::GetFocusCenter() const noexcept
{
	return m_grid.Center(m_focus);
}

RegionMapView::Built RegionMapView::Build(const RegionGenerator &generator, const ColorFunction &color,
                                          const LocalGrid &grid, RegionId focus, int radiusRegions)
{
	Built built;
	built.patch = generator.GeneratePatch(grid, focus, radiusRegions, CellSizeForRadius(radiusRegions));
	const RegionPatch &patch = built.patch;
	const int points = patch.points;
	const double cellSize = patch.cellSizeMeters;
	built.pixels.resize(static_cast<std::size_t>(points) * points);

	const auto height = [&](int x, int y) {
		return patch.ElevationAt(std::clamp(x, 0, points - 1), std::clamp(y, 0, points - 1));
	};

	// Light from the upper left; a flat surface keeps its base colour.
	const float lightX = -0.5f;
	const float lightY = 0.6f;
	const float lightZ = 0.62f;
	const float lightLength = std::sqrt(lightX * lightX + lightY * lightY + lightZ * lightZ);

	for (int row = 0; row < points; ++row)
	{
		const int y = points - 1 - row; // image up is the face v axis
		for (int x = 0; x < points; ++x)
		{
			const std::size_t index = patch.Index(x, y);
			const float elevation = patch.elevationMeters[index];
			const Biome biome = patch.biome[index];
			const PlanetSurface &surface = patch.surface[index];
			PlanetRegion cell = surface.ToRegion(elevation);
			cell.biome = biome;

			Color base;
			bool isWater = false;
			float waterDepth = 0.0f;

			// The sea is the zero-height contour, not an ocean cell's border.
			if (elevation < 0.0f)
			{
				isWater = true;
				waterDepth = -elevation;
			}
			else if (surface.lakeFraction > 0.0)
			{
				const float lakeLevel = static_cast<float>(surface.lakeWaterLevelKm * 1000.0);
				if (elevation < lakeLevel)
				{
					isWater = true;
					waterDepth = lakeLevel - elevation;
				}
			}

			if (isWater)
			{
				base = Lerp({64, 150, 190, 255}, {18, 52, 110, 255}, waterDepth / 200.0f);
			}
			else if (IsAquatic(biome))
			{
				base = {196, 184, 140, 255}; // an aquatic biome above the water line: shore
			}
			else
			{
				base = color(cell, elevation);
			}

			const LocalWater water = generator.GetTerrain().WaterAt(
			    PlanetaryCoordinates::ToGeographic(patch.Direction(x, y)), cellSize * 2.0);
			if (water.origin == FluidOrigin::Lake || water.origin == FluidOrigin::River)
			{
				if (elevation < static_cast<float>(water.surfaceMeters))
				{
					isWater = true;
					waterDepth = static_cast<float>(water.surfaceMeters) - elevation;
				}
			}

			const float slopeX = (height(x + 1, y) - height(x - 1, y)) / static_cast<float>(2.0 * cellSize);
			const float slopeY = (height(x, y + 1) - height(x, y - 1)) / static_cast<float>(2.0 * cellSize);
			const float nx = -slopeX * kReliefExaggeration;
			const float ny = -slopeY * kReliefExaggeration;
			const float normalLength = std::sqrt(nx * nx + ny * ny + 1.0f);
			const float lit = (nx * lightX + ny * lightY + lightZ) / (normalLength * lightLength);
			const float flat = lightZ / lightLength;
			const float brightness = std::clamp(1.0f + 1.8f * (lit - flat), 0.45f, 1.5f);

			built.pixels[static_cast<std::size_t>(row) * points + x] = {
			    static_cast<unsigned char>(std::clamp(base.r * brightness, 0.0f, 255.0f)),
			    static_cast<unsigned char>(std::clamp(base.g * brightness, 0.0f, 255.0f)),
			    static_cast<unsigned char>(std::clamp(base.b * brightness, 0.0f, 255.0f)), 255};
		}
	}
	return built;
}

void RegionMapView::Update()
{
	// The cache owns the generation. A miss returns nullptr and the previous
	// theater stays on screen; the next frame polls again.
	if (m_requestedTheater.has_value() && IsAnchoring())
	{
		if (std::shared_ptr<const WorldMap> world = m_theaters.GetOrCreate(*m_requestedTheater))
		{
			ApplyTheater(*m_requestedTheater, std::move(world));
		}
	}

	if (m_pending.valid() && m_pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
	{
		Built built = m_pending.get();
		// A theater switch or a new zoom may have happened while the worker was
		// busy; dropping the result avoids showing the previous area.
		if (m_pendingFocus == m_focus && m_pendingVersion == m_gridVersion && m_pendingRadius == m_patchRadius)
		{
			const int points = built.patch.points;
			const Image image{built.pixels.data(), points, points, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
			const Texture2D texture = LoadTextureFromImage(image);
			if (texture.id != 0)
			{
				SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
				m_texture = UniqueTexture(texture);
				m_patch = std::make_shared<const RegionPatch>(std::move(built.patch));
				m_displayed = m_pendingFocus;
				m_displayedVersion = m_pendingVersion;
				m_displayedRadius = built.patch.radiusRegions;
				m_hasImage = true;
			}
		}
	}

	if (!m_pending.valid() && m_worldMap != nullptr && !IsAnchoring() &&
	    (!m_hasImage || !(m_displayed == m_focus) || m_displayedVersion != m_gridVersion ||
	     m_displayedRadius != m_patchRadius))
	{
		m_pendingFocus = m_focus;
		m_pendingRadius = m_patchRadius;
		m_pendingVersion = m_gridVersion;
		// Copy the generator: the world pointer inside keeps the theater alive
		// even if a new one is applied while this worker is still running.
		m_pending = std::async(std::launch::async,
		                       [generator = m_generator, color = m_color, focus = m_focus, grid = m_grid,
		                        radius = m_pendingRadius] { return Build(generator, color, grid, focus, radius); });
	}
}

Rectangle RegionMapView::FitSquare(Rectangle bounds) noexcept
{
	const float side = std::max(1.0f, std::min(bounds.width, bounds.height));
	return {bounds.x + (bounds.width - side) * 0.5f, bounds.y + (bounds.height - side) * 0.5f, side, side};
}

bool RegionMapView::PatchPoint(Vector2 mouse, Rectangle bounds, int &x, int &y) const
{
	if (!m_hasImage)
	{
		return false;
	}
	const Rectangle square = FitSquare(bounds);
	if (mouse.x < square.x || mouse.x >= square.x + square.width || mouse.y < square.y ||
	    mouse.y >= square.y + square.height)
	{
		return false;
	}
	const int last = m_patch->points - 1;
	x = std::clamp(static_cast<int>(std::lround((mouse.x - square.x) / square.width * last)), 0, last);
	y = std::clamp(static_cast<int>(std::lround(last - (mouse.y - square.y) / square.height * last)), 0, last);
	return true;
}

std::optional<CartesianPoint3D> RegionMapView::DirectionAt(Vector2 mouse, Rectangle bounds) const
{
	if (!m_hasImage)
	{
		return std::nullopt;
	}
	const Rectangle square = FitSquare(bounds);
	if (mouse.x < square.x || mouse.x >= square.x + square.width || mouse.y < square.y ||
	    mouse.y >= square.y + square.height)
	{
		return std::nullopt;
	}
	const int last = m_patch->points - 1;
	const double x = (mouse.x - square.x) / square.width * last;
	const double y = last - (mouse.y - square.y) / square.height * last;
	return m_patch->Direction(x, y);
}

std::optional<float> RegionMapView::ElevationAt(Vector2 mouse, Rectangle bounds) const
{
	int x = 0;
	int y = 0;
	if (!PatchPoint(mouse, bounds, x, y))
	{
		return std::nullopt;
	}
	return m_patch->ElevationAt(x, y);
}

std::optional<Biome> RegionMapView::BiomeAt(Vector2 mouse, Rectangle bounds) const
{
	int x = 0;
	int y = 0;
	if (!PatchPoint(mouse, bounds, x, y))
	{
		return std::nullopt;
	}
	return m_patch->biome[m_patch->Index(x, y)];
}

double RegionMapView::PatchSpanMeters() const noexcept
{
	return m_patch ? static_cast<double>(m_patch->points - 1) * m_patch->cellSizeMeters : 0.0;
}

std::optional<Biome> RegionMapView::FocusBiome() const
{
	if (!m_patch)
	{
		return std::nullopt;
	}
	double x = 0.0;
	double y = 0.0;
	m_patch->Project(m_patch->grid.CenterDirection(m_focus), x, y);
	const int px = std::clamp(static_cast<int>(std::lround(x)), 0, m_patch->points - 1);
	const int py = std::clamp(static_cast<int>(std::lround(y)), 0, m_patch->points - 1);
	return m_patch->biome[m_patch->Index(px, py)];
}

void RegionMapView::Zoom(int steps)
{
	static constexpr int kRadii[] = {1, 2, 4, 8};
	int index = 0;
	for (int candidate = 1; candidate < static_cast<int>(std::size(kRadii)); ++candidate)
	{
		if (kRadii[candidate] <= m_patchRadius)
		{
			index = candidate;
		}
	}
	index = std::clamp(index + steps, 0, static_cast<int>(std::size(kRadii)) - 1);
	if (kRadii[index] != m_patchRadius)
	{
		m_patchRadius = kRadii[index];
		++m_gridVersion;
	}
}

void RegionMapView::Draw(Rectangle bounds) const
{
	const Rectangle square = FitSquare(bounds);
	const bool stale = m_hasImage && (m_displayed != m_focus || m_displayedVersion != m_gridVersion ||
	                                  m_displayedRadius != m_patchRadius);
	if (!m_hasImage || stale)
	{
		DrawRectangleRec(square, {14, 21, 31, 255});
		DrawText("Generating region...", static_cast<int>(square.x + 16.0f), static_cast<int>(square.y + 16.0f), 18,
		         {139, 158, 177, 255});
		return;
	}

	const int points = m_patch->points;
	const int last = points - 1;
	DrawTexturePro(m_texture.Get(), {0.0f, 0.0f, static_cast<float>(points), static_cast<float>(points)}, square,
	               {0.0f, 0.0f}, 0.0f, WHITE);

	// Region borders: an exact chessboard, upright on this north-up map. The
	// focus region is outlined.
	const LocalGrid &grid = m_patch->grid;
	const auto toScreen = [&](double x, double y) {
		return Vector2{square.x + static_cast<float>(x / last) * square.width,
		               square.y + static_cast<float>(1.0 - y / last) * square.height};
	};
	const auto outline = [&](RegionId id, float thickness, Color color) {
		constexpr int edge = LocalGrid::kCellsPerRegionEdge;
		Vector2 corners[4];
		const int cornerCells[4][2] = {{0, 0}, {edge, 0}, {edge, edge}, {0, edge}};
		for (int index = 0; index < 4; ++index)
		{
			double x = 0.0;
			double y = 0.0;
			m_patch->Project(grid.PointDirection(id, cornerCells[index][0], cornerCells[index][1]), x, y);
			corners[index] = toScreen(x, y);
		}
		for (int side = 0; side < 4; ++side)
		{
			DrawLineEx(corners[side], corners[(side + 1) % 4], thickness, color);
		}
	};

	BeginScissorMode(static_cast<int>(square.x), static_cast<int>(square.y), static_cast<int>(square.width),
	                 static_cast<int>(square.height));
	const RegionId middle = m_patch->center;
	outline(middle, 2.0f, kFocusColor);
	EndScissorMode();
	DrawRectangleLinesEx(square, 1.0f, {129, 158, 180, 255});

	// Scale bar: a round distance that fits the current span.
	const double spanMeters = static_cast<double>(last) * m_patch->cellSizeMeters;
	double barMeters = 1000.0;
	if (spanMeters > 3000.0)
	{
		barMeters = 2000.0;
	}
	if (spanMeters > 8000.0)
	{
		barMeters = 5000.0;
	}
	if (spanMeters > 15000.0)
	{
		barMeters = 10000.0;
	}
	const float barPixels = static_cast<float>(barMeters / spanMeters) * square.width;
	const float barY = square.y + square.height - 18.0f;
	DrawRectangle(static_cast<int>(square.x + 12.0f), static_cast<int>(barY), static_cast<int>(barPixels), 3, RAYWHITE);
	char scaleText[32];
	std::snprintf(scaleText, sizeof(scaleText), "%.0f km", barMeters / 1000.0);
	DrawText(scaleText, static_cast<int>(square.x + 12.0f), static_cast<int>(barY - 16.0f), 14, RAYWHITE);

	// The map is north-up, so the arrow is fixed.
	const Vector2 arrowBase{square.x + 34.0f, square.y + 52.0f};
	DrawLineEx(arrowBase, {arrowBase.x, arrowBase.y - 24.0f}, 2.0f, RAYWHITE);
	DrawTriangle({arrowBase.x, arrowBase.y - 30.0f}, {arrowBase.x - 4.0f, arrowBase.y - 22.0f},
	             {arrowBase.x + 4.0f, arrowBase.y - 22.0f}, RAYWHITE);
	DrawText("N", static_cast<int>(arrowBase.x - 4.0f), static_cast<int>(arrowBase.y - 48.0f), 14, RAYWHITE);

	if (m_pending.valid())
	{
		DrawText("Generating region...", static_cast<int>(square.x + 12.0f),
		         static_cast<int>(square.y + square.height - 40.0f - 16.0f), 14, kFocusColor);
	}
}

void RegionMapView::ReleaseRenderResources() noexcept
{
	m_texture.Reset();
	m_hasImage = false;
	m_patch.reset();
}
} // namespace Infinitesia
