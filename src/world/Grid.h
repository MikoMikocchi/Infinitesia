#pragma once

namespace Infinitesia
{
    struct GridConfig
    {
        int columns;
        int rows;
        float cellSize;
    };

    struct GridCell
    {
        int column;
        int row;
    };

    struct WorldPosition
    {
        float x;
        float y;
        float z;
    };

    class Grid
    {
    public:
        explicit Grid(GridConfig config);

        bool Contains(GridCell cell) const;
        WorldPosition CellToWorld(GridCell cell) const;

        int GetColumns() const;
        int GetRows() const;
        float GetCellSize() const;
        float GetWorldWidth() const;
        float GetWorldDepth() const;

    private:
        GridConfig m_config;
        float m_worldWidth;
        float m_worldDepth;
    };
}
