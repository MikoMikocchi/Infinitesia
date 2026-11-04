#pragma once

#include <cstddef>
#include <optional>

namespace Infinitesia {
struct GridConfig {
  int width;
  int height;
  int depth;
  float cellSize;
};

struct GridCell {
  int x;
  int y;
  int z;
};

struct WorldPosition {
  float x;
  float y;
  float z;
};

class Grid {
 public:
  explicit Grid(GridConfig config);

  bool Contains(GridCell cell) const;
  WorldPosition CellToWorld(GridCell cell) const;
  WorldPosition CellToSurface(GridCell cell) const;
  std::optional<GridCell> WorldToCell(WorldPosition position) const;

  int GetWidth() const;
  int GetHeight() const;
  int GetDepth() const;
  std::size_t GetCellCount() const;
  float GetCellSize() const;
  float GetWorldWidth() const;
  float GetWorldHeight() const;
  float GetWorldDepth() const;

 private:
  GridConfig m_config;
  float m_worldWidth;
  float m_worldHeight;
  float m_worldDepth;
  std::size_t m_cellCount;
};
}  // namespace Infinitesia
