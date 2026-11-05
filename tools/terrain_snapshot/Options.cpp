#include "terrain_snapshot/Options.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "domains/natural/planetary/region/LocalGrid.h"

namespace SnapshotTool
{
using namespace Infinitesia;

void PrintUsage()
{
	std::puts("TerrainSnapshot --lat <deg> --lon <deg> [options]\n"
	          "\n"
	          "Place (required): the settlement anchor on the planet; the window is "
	          "centred on it\n"
	          "  --lat, --lon <deg>     latitude -90..90, longitude 0..360 (east "
	          "positive)\n"
	          "  --seed <n>             planet seed (the game uses 1)\n"
	          "  --planet-bands <n>     coarser planet grid for a quick look (not the "
	          "game's terrain)\n"
	          "\n"
	          "Window:\n"
	          "  --extent <m>           side of the square window in metres (default "
	          "256)\n"
	          "  --chunks <n>           window of n x n chunks (16 m each); same as "
	          "--extent 16n\n"
	          "  --region <i,j>         the whole 4096 m region i east, j north of the "
	          "anchor\n"
	          "  --east <m>, --north <m>  move the window centre away from the anchor\n"
	          "  --step <m>             metres per column (default: 1 up to 256 m, "
	          "then extent / 256)\n"
	          "\n"
	          "Picture:\n"
	          "  --view se|sw|ne|nw     which corner to look from (default se)\n"
	          "  --cut <f>              keep only the far fraction f (0..1) of the "
	          "window along both\n"
	          "                         sides, so the cut faces show the inside; "
	          "--cut-u / --cut-v for one\n"
	          "  --depth <m>            ground drawn below the lowest surface (default "
	          "extent / 8)\n"
	          "  --scale <px>           pixels per metre (default: about 1600 px "
	          "wide)\n"
	          "  --vexag <f>            vertical exaggeration (default grows with the "
	          "window)\n"
	          "  --out <file.png>       output (default snapshot.png)\n"
	          "  --map                  a geological map from above (rock under the "
	          "topsoil, contacts as\n"
	          "                         dark lines, hill shading) instead of the block "
	          "picture; faults are drawn\n"
	          "                         where their plane reaches the ground: red "
	          "normal, blue thrust, yellow\n"
	          "                         strike-slip\n"
	          "  --assets <dir>         the assets folder (default: the source tree's "
	          "assets)\n"
	          "\n"
	          "Every block is drawn in the colour of its material, taken from the "
	          "material catalog; the rocks\n"
	          "in view are listed with their colours after the picture is written.\n");
}

bool ParseOptions(int argc, char **argv, Options &options)
{
	for (int index = 1; index < argc; ++index)
	{
		const std::string key = argv[index];
		const auto value = [&]() -> const char * {
			if (index + 1 >= argc)
			{
				throw std::invalid_argument("Missing a value after " + key);
			}
			return argv[++index];
		};

		if (key == "--help" || key == "-h")
		{
			return false;
		}
		else if (key == "--lat")
		{
			options.latitude = std::atof(value());
			options.hasPlace = true;
		}
		else if (key == "--lon")
		{
			options.longitude = std::atof(value());
			options.hasPlace = true;
		}
		else if (key == "--seed")
		{
			options.seed = std::strtoull(value(), nullptr, 10);
		}
		else if (key == "--planet-bands")
		{
			options.planetBands = std::atoi(value());
		}
		else if (key == "--extent")
		{
			options.extentMeters = std::atof(value());
		}
		else if (key == "--chunks")
		{
			options.extentMeters = 16.0 * std::atof(value());
		}
		else if (key == "--region")
		{
			int i = 0;
			int j = 0;
			if (std::sscanf(value(), "%d,%d", &i, &j) != 2)
			{
				throw std::invalid_argument("--region needs i,j");
			}
			options.extentMeters = LocalGrid::kRegionSizeMeters;
			options.originEast = i * LocalGrid::kRegionSizeMeters;
			options.originNorth = j * LocalGrid::kRegionSizeMeters;
		}
		else if (key == "--east")
		{
			options.originEast = std::atof(value());
		}
		else if (key == "--north")
		{
			options.originNorth = std::atof(value());
		}
		else if (key == "--step")
		{
			options.stepMeters = std::atof(value());
		}
		else if (key == "--view")
		{
			options.view = value();
		}
		else if (key == "--cut")
		{
			options.cutU = options.cutV = std::atof(value());
		}
		else if (key == "--cut-u")
		{
			options.cutU = std::atof(value());
		}
		else if (key == "--cut-v")
		{
			options.cutV = std::atof(value());
		}
		else if (key == "--depth")
		{
			options.depthMeters = std::atof(value());
		}
		else if (key == "--scale")
		{
			options.scale = std::atof(value());
		}
		else if (key == "--vexag")
		{
			options.verticalExaggeration = std::atof(value());
		}
		else if (key == "--out")
		{
			options.output = value();
		}
		else if (key == "--assets")
		{
			options.assets = value();
		}
		else if (key == "--map")
		{
			options.map = true;
		}
		else
		{
			throw std::invalid_argument("Unknown option " + key);
		}
	}
	return true;
}

bool ValidateOptions(const Options &options)
{
	if (!options.hasPlace)
	{
		std::fprintf(stderr, "Give a place with --lat and --lon (see --help).\n");
		return false;
	}
	if (options.view != "se" && options.view != "sw" && options.view != "ne" && options.view != "nw")
	{
		std::fprintf(stderr, "--view must be se, sw, ne or nw.\n");
		return false;
	}
	if (!(options.extentMeters >= 2.0) || options.cutU <= 0.0 || options.cutU > 1.0 || options.cutV <= 0.0 ||
	    options.cutV > 1.0)
	{
		std::fprintf(stderr, "Extent must be at least 2 m and cuts must be in (0, 1].\n");
		return false;
	}

	const double extent = options.extentMeters;
	double step = options.stepMeters;
	if (step <= 0.0)
	{
		step = std::max(1.0, std::ceil(extent / 256.0));
	}
	const int columns = std::max(1, static_cast<int>(std::lround(extent / step)));
	if (static_cast<long long>(columns) * columns > (1LL << 21))
	{
		std::fprintf(stderr,
		             "%d x %d columns is too many; use a larger --step or a "
		             "smaller --extent.\n",
		             columns, columns);
		return false;
	}
	return true;
}
} // namespace SnapshotTool
