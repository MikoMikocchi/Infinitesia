#include "gameplay/StartingCells.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace Infinitesia
{
    namespace
    {
        constexpr int kMinTreeDistance = 3;
        constexpr int kMaxTreeDistance = 16;
        constexpr int kSide = 2 * kMaxTreeDistance + 1;
    }

    int SurfaceLevelOf(const Grid& grid, const TerrainSource& terrain, int x, int z)
    {
        if (x < 0 || x >= grid.GetWidth() || z < 0 || z >= grid.GetDepth())
        {
            return -1;
        }
        const BlockRegistry& registry = terrain.Registry();
        for (int y = grid.GetHeight() - 1; y >= 0; --y)
        {
            const BlockState state = terrain.StateAt({ x, y, z });
            // Ground to stand on: something is there and it is neither water nor a plant.
            if (registry.Occupied(state) && !registry.Has(state.material, Tag::Water) &&
                !registry.Has(state.material, Tag::Plant))
            {
                return y;
            }
        }
        return -1;
    }

    StartingCells FindStartingCells(const Grid& grid, const TerrainSource& terrain)
    {
        const int centerX = grid.GetWidth() / 2;
        const int centerZ = grid.GetDepth() / 2;
        const int level = SurfaceLevelOf(grid, terrain, centerX, centerZ);
        StartingCells cells{ { centerX, level, centerZ }, std::nullopt };

        // Surface levels around the centre, looked up once.
        const auto indexOf = [](int dx, int dz)
        {
            return static_cast<std::size_t>(dz + kMaxTreeDistance) * kSide +
                   static_cast<std::size_t>(dx + kMaxTreeDistance);
        };
        std::vector<int> surface(static_cast<std::size_t>(kSide) * kSide);
        for (int dz = -kMaxTreeDistance; dz <= kMaxTreeDistance; ++dz)
        {
            for (int dx = -kMaxTreeDistance; dx <= kMaxTreeDistance; ++dx)
            {
                surface[indexOf(dx, dz)] = SurfaceLevelOf(grid, terrain, centerX + dx, centerZ + dz);
            }
        }
        const auto reachable = [&](int dx, int dz)
        {
            for (int step = 0; step <= std::abs(dx); ++step)
            {
                if (surface[indexOf(dx < 0 ? -step : step, 0)] < level)
                {
                    return false;
                }
            }
            for (int step = 0; step <= std::abs(dz); ++step)
            {
                if (surface[indexOf(dx, dz < 0 ? -step : step)] < level)
                {
                    return false;
                }
            }
            return true;
        };

        for (int radius = kMinTreeDistance; radius <= kMaxTreeDistance; ++radius)
        {
            for (int dz = -radius; dz <= radius; ++dz)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (std::max(std::abs(dx), std::abs(dz)) == radius &&
                        surface[indexOf(dx, dz)] == level && reachable(dx, dz) &&
                        !terrain.Registry().Occupied(terrain.StateAt({ centerX + dx, level + 1, centerZ + dz })))
                    {
                        cells.tree = GridCell{ centerX + dx, level, centerZ + dz };
                        return cells;
                    }
                }
            }
        }
        return cells;
    }
}
