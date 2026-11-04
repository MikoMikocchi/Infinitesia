#include "presentation/rendering/WorldOverlayRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "world/Chunk.h"

namespace Infinitesia {
namespace WorldOverlayRenderer {
namespace {
constexpr float kGridLineHeight = 0.01f;
constexpr float kHighlightHeight = 0.005f;
constexpr Color kGridLineColor = {85, 95, 105, 255};
constexpr Color kGridBoundaryColor = {145, 155, 165, 255};
constexpr Color kHoveredCellColor = {245, 190, 55, 100};
constexpr Color kHoveredCellOutlineColor = {255, 215, 90, 255};
constexpr Color kHoveredObjectOutlineColor = {245, 245, 245, 255};
constexpr Color kSelectedObjectOutlineColor = {70, 220, 110, 255};
constexpr Color kDebugPathColor = {70, 205, 235, 230};
constexpr Color kDebugPathBendColor = {255, 195, 65, 255};
constexpr float kDebugPathHeight = 0.075f;

void DrawObjectOutline(const Grid& grid, const WorldObject& object,
                       Color color) {
  const WorldPosition center = object.position;
  const WorldObjectSize size =
      GetWorldObjectSize(*object.kind, grid.GetCellSize());

  DrawBoundingBox(
      {{center.x - size.width * 0.5f, center.y, center.z - size.width * 0.5f},
       {center.x + size.width * 0.5f, center.y + size.height,
        center.z + size.width * 0.5f}},
      color);
}
}  // namespace

void DrawGridLines(const Grid& grid, GridCell viewCenter, int chunkRadius,
                   bool showCellGrid, int viewLevel) {
  const int firstColumn =
      std::max(0, (viewCenter.x / kChunkSize - chunkRadius) * kChunkSize);
  const int lastColumn =
      std::min(grid.GetWidth(),
               (viewCenter.x / kChunkSize + chunkRadius + 1) * kChunkSize);
  const int firstRow =
      std::max(0, (viewCenter.z / kChunkSize - chunkRadius) * kChunkSize);
  const int lastRow =
      std::min(grid.GetDepth(),
               (viewCenter.z / kChunkSize + chunkRadius + 1) * kChunkSize);
  const float firstX = -grid.GetWorldWidth() * 0.5f +
                       static_cast<float>(firstColumn) * grid.GetCellSize();
  const float lastX = -grid.GetWorldWidth() * 0.5f +
                      static_cast<float>(lastColumn) * grid.GetCellSize();
  const float firstZ = -grid.GetWorldDepth() * 0.5f +
                       static_cast<float>(firstRow) * grid.GetCellSize();
  const float lastZ = -grid.GetWorldDepth() * 0.5f +
                      static_cast<float>(lastRow) * grid.GetCellSize();
  const float lineY =
      static_cast<float>(viewLevel + 1) * grid.GetCellSize() + kGridLineHeight;
  const int lineSpacing = showCellGrid ? 1 : kChunkSize;

  for (int column = firstColumn; column < lastColumn; column += lineSpacing) {
    const float x = -grid.GetWorldWidth() * 0.5f +
                    static_cast<float>(column) * grid.GetCellSize();
    const Color color = (column == 0 || column == grid.GetWidth())
                            ? kGridBoundaryColor
                            : kGridLineColor;
    DrawLine3D({x, lineY, firstZ}, {x, lineY, lastZ}, color);
  }
  const float finalX = -grid.GetWorldWidth() * 0.5f +
                       static_cast<float>(lastColumn) * grid.GetCellSize();
  DrawLine3D(
      {finalX, lineY, firstZ}, {finalX, lineY, lastZ},
      lastColumn == grid.GetWidth() ? kGridBoundaryColor : kGridLineColor);

  for (int row = firstRow; row < lastRow; row += lineSpacing) {
    const float z = -grid.GetWorldDepth() * 0.5f +
                    static_cast<float>(row) * grid.GetCellSize();
    const Color color = (row == 0 || row == grid.GetDepth())
                            ? kGridBoundaryColor
                            : kGridLineColor;
    DrawLine3D({firstX, lineY, z}, {lastX, lineY, z}, color);
  }
  const float finalZ = -grid.GetWorldDepth() * 0.5f +
                       static_cast<float>(lastRow) * grid.GetCellSize();
  DrawLine3D({firstX, lineY, finalZ}, {lastX, lineY, finalZ},
             lastRow == grid.GetDepth() ? kGridBoundaryColor : kGridLineColor);
}

void DrawHoveredCell(const Grid& grid, GridCell cell) {
  if (!grid.Contains(cell)) {
    return;
  }

  const WorldPosition center = grid.CellToSurface(cell);
  const float halfCell = grid.GetCellSize() * 0.5f;
  const float left = center.x - halfCell;
  const float right = center.x + halfCell;
  const float top = center.z - halfCell;
  const float bottom = center.z + halfCell;

  DrawPlane({center.x, center.y + kHighlightHeight, center.z},
            {grid.GetCellSize(), grid.GetCellSize()}, kHoveredCellColor);
  const float outlineY = center.y + kHighlightHeight;
  DrawLine3D({left, outlineY, top}, {right, outlineY, top},
             kHoveredCellOutlineColor);
  DrawLine3D({right, outlineY, top}, {right, outlineY, bottom},
             kHoveredCellOutlineColor);
  DrawLine3D({right, outlineY, bottom}, {left, outlineY, bottom},
             kHoveredCellOutlineColor);
  DrawLine3D({left, outlineY, bottom}, {left, outlineY, top},
             kHoveredCellOutlineColor);
}

void DrawDebugPath(const Grid& grid, WorldPosition origin,
                   const std::vector<GridCell>& path, int viewLevel,
                   ViewMode mode) {
  if (path.empty()) {
    return;
  }

  const float bendRadius = grid.GetCellSize() * 0.12f;
  // Parts of the path above the cut are not drawn: they would hang in empty
  // air.
  std::vector<Vector3> points;
  std::vector<bool> shown;
  points.reserve(path.size() + 1);
  shown.reserve(path.size() + 1);
  points.push_back({origin.x, origin.y + kDebugPathHeight, origin.z});
  shown.push_back(mode == ViewMode::ShowAll ||
                  origin.y <
                      static_cast<float>(viewLevel + 1) * grid.GetCellSize());
  for (const GridCell cell : path) {
    shown.push_back(IsLevelShown(mode, cell.y, viewLevel));
    const WorldPosition position = grid.CellToSurface(cell);
    points.push_back({position.x, position.y + kDebugPathHeight, position.z});
  }

  for (std::size_t index = 1; index < points.size(); ++index) {
    if (shown[index - 1] && shown[index]) {
      DrawLine3D(points[index - 1], points[index], kDebugPathColor);
    }
  }

  for (std::size_t index = 1; index + 1 < points.size(); ++index) {
    const Vector3 incoming{points[index].x - points[index - 1].x, 0.0f,
                           points[index].z - points[index - 1].z};
    const Vector3 outgoing{points[index + 1].x - points[index].x, 0.0f,
                           points[index + 1].z - points[index].z};
    if (shown[index] &&
        std::abs(incoming.x * outgoing.z - incoming.z * outgoing.x) > 1.0e-5f) {
      DrawSphere(
          {points[index].x, kDebugPathHeight + bendRadius, points[index].z},
          bendRadius, kDebugPathBendColor);
    }
  }
}

void DrawObjectHighlights(const Grid& grid, const EntityStore& world,
                          std::optional<WorldObjectId> hoveredObject,
                          std::optional<WorldObjectId> selectedObject) {
  for (const WorldObject& object : world.GetObjects()) {
    const bool isSelected =
        selectedObject.has_value() && object.id == *selectedObject;
    const bool isHovered =
        hoveredObject.has_value() && object.id == *hoveredObject;
    if (isSelected) {
      DrawObjectOutline(grid, object, kSelectedObjectOutlineColor);
    } else if (isHovered) {
      DrawObjectOutline(grid, object, kHoveredObjectOutlineColor);
    }
  }
}
}  // namespace WorldOverlayRenderer
}  // namespace Infinitesia
