#include "world/Grid.h"

#include <cmath>
#include <stdexcept>

namespace Infinitesia
{
    Grid::Grid(GridConfig config)
        : m_config(config),
          m_worldWidth(static_cast<float>(config.columns) * config.cellSize),
          m_worldDepth(static_cast<float>(config.rows) * config.cellSize)
    {
        if (config.columns <= 0 || config.rows <= 0)
        {
            throw std::invalid_argument("Grid dimensions must be greater than zero.");
        }

        if (!std::isfinite(config.cellSize) || config.cellSize <= 0.0f)
        {
            throw std::invalid_argument("Grid cell size must be finite and greater than zero.");
        }

        if (!std::isfinite(m_worldWidth) || !std::isfinite(m_worldDepth))
        {
            throw std::invalid_argument("Grid world dimensions must be finite.");
        }
    }

    bool Grid::Contains(GridCell cell) const
    {
        return cell.column >= 0 && cell.column < m_config.columns &&
               cell.row >= 0 && cell.row < m_config.rows;
    }

    WorldPosition Grid::CellToWorld(GridCell cell) const
    {
        if (!Contains(cell))
        {
            throw std::out_of_range("Grid cell is outside the grid bounds.");
        }

        return {
            (static_cast<float>(cell.column) + 0.5f) * m_config.cellSize - m_worldWidth * 0.5f,
            0.0f,
            (static_cast<float>(cell.row) + 0.5f) * m_config.cellSize - m_worldDepth * 0.5f
        };
    }

    std::optional<GridCell> Grid::WorldToCell(WorldPosition position) const
    {
        if (!std::isfinite(position.x) || !std::isfinite(position.z))
        {
            return std::nullopt;
        }

        const float localX = position.x + m_worldWidth * 0.5f;
        const float localZ = position.z + m_worldDepth * 0.5f;
        if (localX < 0.0f || localX >= m_worldWidth ||
            localZ < 0.0f || localZ >= m_worldDepth)
        {
            return std::nullopt;
        }

        const GridCell cell{
            static_cast<int>(localX / m_config.cellSize),
            static_cast<int>(localZ / m_config.cellSize)
        };
        return Contains(cell) ? std::optional<GridCell>{ cell } : std::nullopt;
    }

    int Grid::GetColumns() const
    {
        return m_config.columns;
    }

    int Grid::GetRows() const
    {
        return m_config.rows;
    }

    float Grid::GetCellSize() const
    {
        return m_config.cellSize;
    }

    float Grid::GetWorldWidth() const
    {
        return m_worldWidth;
    }

    float Grid::GetWorldDepth() const
    {
        return m_worldDepth;
    }
}
