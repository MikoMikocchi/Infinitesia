#include "world/Grid.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace Infinitesia {
Grid::Grid(GridConfig config)
    : m_config(config),
      m_worldWidth(static_cast<float>(config.width) * config.cellSize),
      m_worldHeight(static_cast<float>(config.height) * config.cellSize),
      m_worldDepth(static_cast<float>(config.depth) * config.cellSize),
      m_cellCount(0) {
  if (config.width <= 0 || config.height <= 0 || config.depth <= 0) {
    throw std::invalid_argument("Grid dimensions must be greater than zero.");
  }

  if (!std::isfinite(config.cellSize) || config.cellSize <= 0.0f) {
    throw std::invalid_argument(
        "Grid cell size must be finite and greater than zero.");
  }

  if (!std::isfinite(m_worldWidth) || !std::isfinite(m_worldHeight) ||
      !std::isfinite(m_worldDepth)) {
    throw std::invalid_argument("Grid world dimensions must be finite.");
  }

  const std::size_t width = static_cast<std::size_t>(config.width);
  const std::size_t height = static_cast<std::size_t>(config.height);
  const std::size_t depth = static_cast<std::size_t>(config.depth);
  const std::size_t maxCellCount = std::numeric_limits<std::size_t>::max();
  if (width > maxCellCount / height || width * height > maxCellCount / depth) {
    throw std::overflow_error("Grid cell count is too large.");
  }
  m_cellCount = width * height * depth;
}

bool Grid::Contains(GridCell cell) const {
  return cell.x >= 0 && cell.x < m_config.width && cell.y >= 0 &&
         cell.y < m_config.height && cell.z >= 0 && cell.z < m_config.depth;
}

WorldPosition Grid::CellToWorld(GridCell cell) const {
  if (!Contains(cell)) {
    throw std::out_of_range("Grid cell is outside the grid bounds.");
  }

  return {(static_cast<float>(cell.x) + 0.5f) * m_config.cellSize -
              m_worldWidth * 0.5f,
          (static_cast<float>(cell.y) + 0.5f) * m_config.cellSize,
          (static_cast<float>(cell.z) + 0.5f) * m_config.cellSize -
              m_worldDepth * 0.5f};
}

WorldPosition Grid::CellToSurface(GridCell cell) const {
  if (!Contains(cell)) {
    throw std::out_of_range("Grid cell is outside the grid bounds.");
  }

  WorldPosition position = CellToWorld(cell);
  position.y += m_config.cellSize * 0.5f;
  return position;
}

std::optional<GridCell> Grid::WorldToCell(WorldPosition position) const {
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z)) {
    return std::nullopt;
  }

  const float localX = position.x + m_worldWidth * 0.5f;
  const float localY = position.y;
  const float localZ = position.z + m_worldDepth * 0.5f;
  if (localX < 0.0f || localX >= m_worldWidth || localY < 0.0f ||
      localY > m_worldHeight || localZ < 0.0f || localZ >= m_worldDepth) {
    return std::nullopt;
  }

  const GridCell cell{static_cast<int>(localX / m_config.cellSize),
                      localY == m_worldHeight
                          ? m_config.height - 1
                          : static_cast<int>(localY / m_config.cellSize),
                      static_cast<int>(localZ / m_config.cellSize)};
  return Contains(cell) ? std::optional<GridCell>{cell} : std::nullopt;
}

int Grid::GetWidth() const { return m_config.width; }

int Grid::GetHeight() const { return m_config.height; }

int Grid::GetDepth() const { return m_config.depth; }

std::size_t Grid::GetCellCount() const { return m_cellCount; }

float Grid::GetCellSize() const { return m_config.cellSize; }

float Grid::GetWorldWidth() const { return m_worldWidth; }

float Grid::GetWorldHeight() const { return m_worldHeight; }

float Grid::GetWorldDepth() const { return m_worldDepth; }
}  // namespace Infinitesia
