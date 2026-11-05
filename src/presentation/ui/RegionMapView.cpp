#include "presentation/ui/RegionMapView.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace Infinitesia {
namespace {
constexpr float kReliefExaggeration = 3.0f;
constexpr Color kFocusColor = {255, 202, 87, 255};

Color Lerp(Color first, Color second, float amount) {
  const float t = std::clamp(amount, 0.0f, 1.0f);
  return {static_cast<unsigned char>(first.r + (second.r - first.r) * t),
          static_cast<unsigned char>(first.g + (second.g - first.g) * t),
          static_cast<unsigned char>(first.b + (second.b - first.b) * t), 255};
}

bool IsAquatic(Biome biome) {
  return biome == Biome::DeepOcean || biome == Biome::ShallowSea ||
         biome == Biome::CoralReef || biome == Biome::PolarSea;
}
}  // namespace

RegionMapView::RegionMapView(const PlanetMap& map, ColorFunction color)
    : m_map(map), m_generator(map), m_color(std::move(color)) {
  m_grid = m_generator.MakeGrid(PlanetaryCoordinates::ToCartesian({0.0, 0.0}));
}

RegionMapView::~RegionMapView() {
  // The worker reads m_generator and m_map; wait for it before members go away.
  if (m_pending.valid()) {
    m_pending.wait();
  }
}

void RegionMapView::Anchor(CartesianPoint3D direction) {
  m_grid = m_generator.MakeGrid(direction);
  ++m_gridVersion;
  m_focus = RegionId{};
}

void RegionMapView::Focus(RegionId id) {
  if (LocalGrid::IsValid(id)) {
    m_focus = id;
  }
}

void RegionMapView::FocusAt(CartesianPoint3D direction) {
  Focus(m_grid.RegionAt(direction));
}

GeographicCoordinates RegionMapView::GetFocusCenter() const noexcept {
  return m_grid.Center(m_focus);
}

RegionMapView::Built RegionMapView::Build(const RegionGenerator& generator,
                                          const PlanetMap& map,
                                          const ColorFunction& color,
                                          const LocalGrid& grid,
                                          RegionId focus) {
  Built built;
  built.patch = generator.GeneratePatch(grid, focus, kPatchRadiusRegions);
  const RegionPatch& patch = built.patch;
  const int points = patch.points;
  const double cellSize = patch.cellSizeMeters;
  const int longitudeSegments = map.GetLongitudeSegmentCount();
  built.pixels.resize(static_cast<std::size_t>(points) * points);

  const auto height = [&](int x, int y) {
    return patch.ElevationAt(std::clamp(x, 0, points - 1),
                             std::clamp(y, 0, points - 1));
  };

  // Light from the upper left; a flat surface keeps its base colour.
  const float lightX = -0.5f;
  const float lightY = 0.6f;
  const float lightZ = 0.62f;
  const float lightLength =
      std::sqrt(lightX * lightX + lightY * lightY + lightZ * lightZ);

  for (int row = 0; row < points; ++row) {
    const int y = points - 1 - row;  // image up is the face v axis
    for (int x = 0; x < points; ++x) {
      const std::size_t index = patch.Index(x, y);
      const float elevation = patch.elevationMeters[index];
      const Biome biome = patch.biome[index];
      const std::uint32_t cellIndex = patch.planetCell[index];
      const PlanetRegion& cell =
          map.GetRegion(static_cast<int>(cellIndex) / longitudeSegments,
                        static_cast<int>(cellIndex) % longitudeSegments);

      Color base;
      bool isWater = false;
      float waterDepth = 0.0f;

      if (cell.isOcean) {
        if (elevation < 0.0f) {
          isWater = true;
          waterDepth = -elevation;
        }
      } else if (cell.isLake) {
        const float lakeLevel =
            static_cast<float>(cell.lakeWaterLevelKm * 1000.0);
        if (elevation < lakeLevel) {
          isWater = true;
          waterDepth = lakeLevel - elevation;
        }
      }

      if (isWater) {
        base =
            Lerp({64, 150, 190, 255}, {18, 52, 110, 255}, waterDepth / 200.0f);
      } else if (IsAquatic(biome)) {
        base = {196, 184, 140,
                255};  // shore: the planet cell says sea, this point is dry
      } else {
        base = color(cell, elevation);
      }

      const float slopeX = (height(x + 1, y) - height(x - 1, y)) /
                           static_cast<float>(2.0 * cellSize);
      const float slopeY = (height(x, y + 1) - height(x, y - 1)) /
                           static_cast<float>(2.0 * cellSize);
      const float nx = -slopeX * kReliefExaggeration;
      const float ny = -slopeY * kReliefExaggeration;
      const float normalLength = std::sqrt(nx * nx + ny * ny + 1.0f);
      const float lit =
          (nx * lightX + ny * lightY + lightZ) / (normalLength * lightLength);
      const float flat = lightZ / lightLength;
      const float brightness =
          std::clamp(1.0f + 1.8f * (lit - flat), 0.45f, 1.5f);

      built.pixels[static_cast<std::size_t>(row) * points + x] = {
          static_cast<unsigned char>(
              std::clamp(base.r * brightness, 0.0f, 255.0f)),
          static_cast<unsigned char>(
              std::clamp(base.g * brightness, 0.0f, 255.0f)),
          static_cast<unsigned char>(
              std::clamp(base.b * brightness, 0.0f, 255.0f)),
          255};
    }
  }
  return built;
}

void RegionMapView::Update() {
  if (m_pending.valid() && m_pending.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
    Built built = m_pending.get();
    const int points = built.patch.points;
    const Image image{built.pixels.data(), points, points, 1,
                      PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    const Texture2D texture = LoadTextureFromImage(image);
    if (texture.id != 0) {
      SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
      m_texture = UniqueTexture(texture);
      m_patch = std::make_shared<const RegionPatch>(std::move(built.patch));
      m_displayed = m_pendingFocus;
      m_displayedVersion = m_pendingVersion;
      m_hasImage = true;
    }
  }

  if (!m_pending.valid() && (!m_hasImage || !(m_displayed == m_focus) ||
                             m_displayedVersion != m_gridVersion)) {
    m_pendingFocus = m_focus;
    m_pendingVersion = m_gridVersion;
    m_pending =
        std::async(std::launch::async, [this, focus = m_focus, grid = m_grid] {
          return Build(m_generator, m_map, m_color, grid, focus);
        });
  }
}

Rectangle RegionMapView::FitSquare(Rectangle bounds) noexcept {
  const float side = std::max(1.0f, std::min(bounds.width, bounds.height));
  return {bounds.x + (bounds.width - side) * 0.5f,
          bounds.y + (bounds.height - side) * 0.5f, side, side};
}

bool RegionMapView::PatchPoint(Vector2 mouse, Rectangle bounds, int& x,
                               int& y) const {
  if (!m_hasImage) {
    return false;
  }
  const Rectangle square = FitSquare(bounds);
  if (mouse.x < square.x || mouse.x >= square.x + square.width ||
      mouse.y < square.y || mouse.y >= square.y + square.height) {
    return false;
  }
  const int last = m_patch->points - 1;
  x = std::clamp(
      static_cast<int>(std::lround((mouse.x - square.x) / square.width * last)),
      0, last);
  y = std::clamp(static_cast<int>(std::lround(last - (mouse.y - square.y) /
                                                         square.height * last)),
                 0, last);
  return true;
}

std::optional<CartesianPoint3D> RegionMapView::DirectionAt(
    Vector2 mouse, Rectangle bounds) const {
  if (!m_hasImage) {
    return std::nullopt;
  }
  const Rectangle square = FitSquare(bounds);
  if (mouse.x < square.x || mouse.x >= square.x + square.width ||
      mouse.y < square.y || mouse.y >= square.y + square.height) {
    return std::nullopt;
  }
  const int last = m_patch->points - 1;
  const double x = (mouse.x - square.x) / square.width * last;
  const double y = last - (mouse.y - square.y) / square.height * last;
  return m_patch->Direction(x, y);
}

std::optional<float> RegionMapView::ElevationAt(Vector2 mouse,
                                                Rectangle bounds) const {
  int x = 0;
  int y = 0;
  if (!PatchPoint(mouse, bounds, x, y)) {
    return std::nullopt;
  }
  return m_patch->ElevationAt(x, y);
}

std::optional<Biome> RegionMapView::BiomeAt(Vector2 mouse,
                                            Rectangle bounds) const {
  int x = 0;
  int y = 0;
  if (!PatchPoint(mouse, bounds, x, y)) {
    return std::nullopt;
  }
  return m_patch->biome[m_patch->Index(x, y)];
}

void RegionMapView::Draw(Rectangle bounds) const {
  const Rectangle square = FitSquare(bounds);
  if (!m_hasImage) {
    DrawRectangleRec(square, {14, 21, 31, 255});
    DrawText("Generating region...", static_cast<int>(square.x + 16.0f),
             static_cast<int>(square.y + 16.0f), 18, {139, 158, 177, 255});
    return;
  }

  const int points = m_patch->points;
  const int last = points - 1;
  DrawTexturePro(
      m_texture.Get(),
      {0.0f, 0.0f, static_cast<float>(points), static_cast<float>(points)},
      square, {0.0f, 0.0f}, 0.0f, WHITE);

  // Region borders: an exact chessboard, upright on this north-up map. The
  // focus region is outlined.
  const LocalGrid& grid = m_patch->grid;
  const auto toScreen = [&](double x, double y) {
    return Vector2{
        square.x + static_cast<float>(x / last) * square.width,
        square.y + static_cast<float>(1.0 - y / last) * square.height};
  };
  const auto outline = [&](RegionId id, float thickness, Color color) {
    constexpr int edge = LocalGrid::kCellsPerRegionEdge;
    Vector2 corners[4];
    const int cornerCells[4][2] = {{0, 0}, {edge, 0}, {edge, edge}, {0, edge}};
    for (int index = 0; index < 4; ++index) {
      double x = 0.0;
      double y = 0.0;
      m_patch->Project(
          grid.PointDirection(id, cornerCells[index][0], cornerCells[index][1]),
          x, y);
      corners[index] = toScreen(x, y);
    }
    for (int side = 0; side < 4; ++side) {
      DrawLineEx(corners[side], corners[(side + 1) % 4], thickness, color);
    }
  };

  BeginScissorMode(static_cast<int>(square.x), static_cast<int>(square.y),
                   static_cast<int>(square.width),
                   static_cast<int>(square.height));
  const RegionId middle = m_patch->center;
  const int reach = m_patch->radiusRegions;
  for (int j = -reach; j <= reach; ++j) {
    for (int i = -reach; i <= reach; ++i) {
      outline({middle.i + i, middle.j + j}, 1.0f, Fade(RAYWHITE, 0.2f));
    }
  }
  outline(middle, 2.0f, kFocusColor);
  EndScissorMode();
  DrawRectangleLinesEx(square, 1.0f, {129, 158, 180, 255});

  // Scale bar: 1 km.
  const float kilometre =
      static_cast<float>(1000.0 / (m_patch->cellSizeMeters * last)) *
      square.width;
  const float barY = square.y + square.height - 18.0f;
  DrawRectangle(static_cast<int>(square.x + 12.0f), static_cast<int>(barY),
                static_cast<int>(kilometre), 3, RAYWHITE);
  DrawText("1 km", static_cast<int>(square.x + 12.0f),
           static_cast<int>(barY - 16.0f), 14, RAYWHITE);

  // The map is north-up, so the arrow is fixed.
  const Vector2 arrowBase{square.x + 34.0f, square.y + 52.0f};
  DrawLineEx(arrowBase, {arrowBase.x, arrowBase.y - 24.0f}, 2.0f, RAYWHITE);
  DrawTriangle({arrowBase.x, arrowBase.y - 30.0f},
               {arrowBase.x - 4.0f, arrowBase.y - 22.0f},
               {arrowBase.x + 4.0f, arrowBase.y - 22.0f}, RAYWHITE);
  DrawText("N", static_cast<int>(arrowBase.x - 4.0f),
           static_cast<int>(arrowBase.y - 48.0f), 14, RAYWHITE);

  if (m_pending.valid()) {
    DrawText("Generating region...", static_cast<int>(square.x + 12.0f),
             static_cast<int>(square.y + square.height - 40.0f - 16.0f), 14,
             kFocusColor);
  }
}

void RegionMapView::ReleaseRenderResources() noexcept {
  m_texture.Reset();
  m_hasImage = false;
  m_patch.reset();
}
}  // namespace Infinitesia
