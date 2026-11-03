// TerrainSnapshot: an isometric "block diagram" of the terrain the game would build,
// written as a PNG without opening a window.
//
// It shows the ground from above together with the cut faces of the block, so the
// layers under the surface can be read next to the relief. The columns come from the
// same SettlementTerrain and layering the game uses, so what you see is what the game
// generates.
//
//   TerrainSnapshot --lat 46.5 --lon 198.5 --out snap.png          256 m window at a place
//   TerrainSnapshot --lat 46.5 --lon 198.5 --chunks 8              8 x 8 chunks (128 m)
//   TerrainSnapshot --lat 46.5 --lon 198.5 --region 1,0            the whole 4 km region east of the anchor
//   TerrainSnapshot ... --cut 0.5                                  keep the far half: the cut shows the inside
//
// See --help for all options.

#include "terrain_snapshot/BlockDiagram.h"
#include "terrain_snapshot/GeologicalMap.h"
#include "terrain_snapshot/Options.h"
#include "terrain_snapshot/SnapshotWorld.h"

#include <chrono>
#include <cstdio>
#include <exception>

using namespace SnapshotTool;

namespace
{
    int Run(const Options& options)
    {
        if (!ValidateOptions(options))
        {
            return 2;
        }
        Window window;
        if (!MakeWindow(options, window))
        {
            return 2;
        }

        // The planet, built the way the game builds it.
        const auto started = std::chrono::steady_clock::now();
        std::fprintf(stderr, "Generating the planet (seed %llu)...\n", static_cast<unsigned long long>(options.seed));
        const SnapshotWorld world(options);
        std::fprintf(stderr, "Planet ready in %.1f s.\n",
                     std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());

        return options.map ? RenderGeologicalMap(options, window, world)
                           : RenderBlockDiagram(options, window, world);
    }
}

int main(int argc, char** argv)
{
    try
    {
        Options options;
        if (!ParseOptions(argc, argv, options))
        {
            PrintUsage();
            return 0;
        }
        return Run(options);
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
