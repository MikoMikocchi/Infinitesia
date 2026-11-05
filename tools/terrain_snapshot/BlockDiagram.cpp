#include "terrain_snapshot/BlockDiagram.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "terrain_snapshot/Image.h"
#include "terrain_snapshot/Reports.h"
#include "world/PlanetTerrainSource.h"

namespace SnapshotTool
{
using namespace Infinitesia;

namespace
{
struct Column
{
	int ground = 0; // top solid block level, relative to the window centre ground
	int top = 0;    // highest filled level (the sea surface over drowned ground)
	SurfaceKind kind = SurfaceKind::Grass;
	ProvinceId province = kNoProvince;
	int pileTop = 0;                     // elevation of the province's rock pile top, metres above sea level
	GeologyModel::ColumnProfile profile; // the rocks under the column
};

// ------------------------------------------------------------------ rendering

struct View
{
	int columns = 0; // columns along each side of the full window
	int keptU = 0;   // columns kept along the screen-right-down axis
	int keptV = 0;
	double stepMeters = 1.0;
	double scale = 1.0; // pixels per metre
	double exaggeration = 1.0;
	double offsetX = 0.0;
	double offsetY = 0.0;

	Point Project(double u, double v, double level) const
	{
		constexpr double kCos30 = 0.86602540378;
		return {offsetX + (u - v) * stepMeters * kCos30 * scale,
		        offsetY + (u + v) * stepMeters * 0.5 * scale - level * scale * exaggeration};
	}
};

// View axes u (towards the lower right) and v (towards the lower left) as
// window indices: east index e grows eastward, south index s grows southward.
void ViewToWindow(const std::string &view, int n, int u, int v, int &east, int &south)
{
	if (view == "sw")
	{
		east = n - 1 - v;
		south = u;
	}
	else if (view == "nw")
	{
		east = n - 1 - u;
		south = n - 1 - v;
	}
	else if (view == "ne")
	{
		east = v;
		south = n - 1 - u;
	}
	else
	{
		east = u;
		south = v;
	}
}
} // namespace

int RenderBlockDiagram(const Options &options, const Window &window, const SnapshotWorld &world)
{
	const double extent = window.extent;
	const double step = window.step;
	const int columns = window.columns;
	const BlockRegistry &registry = world.Registry();
	const SurfaceMatter surface = SurfaceMatter::Resolve(registry);
	const GeologyModel &geology = world.Geology();
	const SettlementTerrain &terrain = world.Terrain();
	const auto colorOf = [&registry](MatterId id) {
		const RgbColor color = registry.ColorOf(id);
		return Rgb{color.r, color.g, color.b};
	};

	// ---- columns in view coordinates, far corner first
	View view;
	view.columns = columns;
	view.keptU = std::clamp(static_cast<int>(std::ceil(options.cutU * columns)), 1, columns);
	view.keptV = std::clamp(static_cast<int>(std::ceil(options.cutV * columns)), 1, columns);
	view.stepMeters = step;

	const int sea = terrain.SeaLevelBlocks();
	std::vector<Column> cells(static_cast<std::size_t>(view.keptU) * static_cast<std::size_t>(view.keptV));
	const auto at = [&](int u, int v) -> Column & { return cells[static_cast<std::size_t>(v) * view.keptU + u]; };

	int lowestGround = 1 << 30;
	int highestTop = -(1 << 30);
	int drowned = 0;
	for (int v = 0; v < view.keptV; ++v)
	{
		for (int u = 0; u < view.keptU; ++u)
		{
			int east = 0;
			int south = 0;
			ViewToWindow(options.view, columns, u, v, east, south);
			const int eastMeters = static_cast<int>(std::lround(-extent * 0.5 + (east + 0.5) * step));
			const int northMeters = static_cast<int>(std::lround(extent * 0.5 - (south + 0.5) * step));
			const SurfaceColumn column = terrain.Column(eastMeters, northMeters);

			Column &cell = at(u, v);
			cell.ground = column.heightBlocks;
			cell.kind = column.kind;
			cell.province = column.province;
			cell.pileTop = column.pileTopMeters;
			cell.profile = geology.ProfileAt(column.province, options.originEast + eastMeters + 0.5,
			                                 options.originNorth + northMeters + 0.5, column.pileTopMeters);
			// The water surface is one level for the whole body, never the shape of
			// the ground under it. A deep ocean therefore draws a deep water column;
			// use --scale or --depth to keep the picture manageable.
			const int waterTop =
			    column.fluid == FluidOrigin::None
			        ? column.heightBlocks
			        : static_cast<int>(std::ceil(column.fluidLevelMeters)) - terrain.OriginElevationMeters() - 1;
			cell.top =
			    column.fluid == FluidOrigin::None ? column.heightBlocks : std::max(column.heightBlocks, waterTop);
			drowned += column.fluid != FluidOrigin::None ? 1 : 0;
			lowestGround = std::min(lowestGround, cell.ground);
			highestTop = std::max(highestTop, cell.top);
		}
	}

	const double depth = options.depthMeters > 0.0 ? options.depthMeters : std::clamp(extent * 0.125, 16.0, 600.0);
	const int bottom = lowestGround - static_cast<int>(std::ceil(depth));

	view.exaggeration =
	    options.verticalExaggeration > 0.0 ? options.verticalExaggeration : std::clamp(extent / 512.0, 1.0, 12.0);
	view.scale = options.scale > 0.0 ? options.scale : 1600.0 / ((columns + columns) * step * 0.86602540378);

	// ---- canvas
	constexpr int kMargin = 24;
	const Point farCorner = view.Project(0, 0, highestTop + 1);
	const Point leftNear = view.Project(0, view.keptV, bottom);
	const Point rightNear = view.Project(view.keptU, 0, bottom);
	const Point nearCorner = view.Project(view.keptU, view.keptV, bottom);
	const double minX = std::min({farCorner.x, leftNear.x, rightNear.x, nearCorner.x});
	const double maxX = std::max({farCorner.x, leftNear.x, rightNear.x, nearCorner.x});
	const double minY = std::min({farCorner.y, leftNear.y, rightNear.y});
	const double maxY = nearCorner.y;
	view.offsetX = kMargin - minX;
	view.offsetY = kMargin - minY;
	const int width = static_cast<int>(std::ceil(maxX - minX)) + 2 * kMargin;
	const int height = static_cast<int>(std::ceil(maxY - minY)) + 2 * kMargin;
	if (width > 12000 || height > 12000)
	{
		std::fprintf(stderr,
		             "The picture would be %d x %d px (surface %d..%d m, sea level "
		             "%+d, depth %.0f m, "
		             "vertical x%.1f); lower --scale or --vexag.\n",
		             width, height, lowestGround, highestTop, sea, depth, view.exaggeration);
		return 2;
	}
	Image image(width, height, {24, 28, 34});

	// ---- paint far to near
	// What fills a block at `level` (relative to the window centre ground):
	// water above the ground, topsoil and then real rock below it.
	std::map<MatterId, long> rocksSeen;
	const auto materialAt = [&](const Column &cell, int level) -> MatterId {
		if (level > cell.ground)
		{
			return surface.water;
		}
		switch (PlanetTerrainSource::LayerAt(cell.kind, cell.ground - level))
		{
		case PlanetTerrainSource::Layer::Turf:
			return surface.turf;
		case PlanetTerrainSource::Layer::Subsoil:
			return surface.subsoil;
		case PlanetTerrainSource::Layer::Sand:
			return surface.sand;
		case PlanetTerrainSource::Layer::Rock:
			break;
		}
		return GeologyModel::RockAt(cell.profile, terrain.OriginElevationMeters() + level);
	};
	const auto countRock = [&](MatterId id) {
		const MatterDef *material = registry.GetMatter(id);
		if (material != nullptr && material->blockClass == BlockClass::Rock)
		{
			++rocksSeen[id];
		}
	};

	const auto layerColor = [&](const Column &cell, int level) -> Rgb {
		const MatterId id = materialAt(cell, level);
		Rgb color = colorOf(id);
		countRock(id);
		if (id == surface.water)
		{
			color = Mix({96, 170, 205}, {22, 56, 116}, (cell.top - level) / 40.0);
		}
		// The deeper the cut, the darker, so the depth reads at a glance.
		const double below = static_cast<double>(cell.top - level);
		return Shade(color, std::max(0.5, 1.0 - below * (extent <= 512.0 ? 0.006 : 0.0015)));
	};

	const auto drawFace = [&](const Column &cell, int u, int v, bool rightFront, int downTo) {
		// The face of this column towards the camera, from its top down to `downTo`
		// (exclusive).
		const double faceShade = rightFront ? 0.62 : 0.8;
		int level = cell.top;
		while (level > downTo)
		{
			const MatterId type = materialAt(cell, level);
			// Merge blocks of one kind into one quad (colour varies slowly with
			// depth).
			int last = level;
			while (last - 1 > downTo)
			{
				const int next = last - 1;
				const MatterId nextType = materialAt(cell, next);
				if (nextType != type || (cell.top - level) / 8 != (cell.top - next) / 8)
				{
					break;
				}
				last = next;
			}
			const Rgb color = Shade(layerColor(cell, level), faceShade);
			const double upper = level + 1;
			const double lower = last;
			if (rightFront)
			{
				image.FillQuad({view.Project(u + 1, v, upper), view.Project(u + 1, v + 1, upper),
				                view.Project(u + 1, v + 1, lower), view.Project(u + 1, v, lower)},
				               color);
			}
			else
			{
				image.FillQuad({view.Project(u, v + 1, upper), view.Project(u + 1, v + 1, upper),
				                view.Project(u + 1, v + 1, lower), view.Project(u, v + 1, lower)},
				               color);
			}
			level = last - 1;
		}
	};

	const double heightSpan = std::max(1, highestTop - lowestGround);
	for (int v = 0; v < view.keptV; ++v)
	{
		for (int u = 0; u < view.keptU; ++u)
		{
			const Column &cell = at(u, v);

			// Towards the lower right, then the lower left; the outer sides go to the
			// bottom.
			const int rightTop = (u + 1 < view.keptU) ? at(u + 1, v).top : bottom - 1;
			const int leftTop = (v + 1 < view.keptV) ? at(u, v + 1).top : bottom - 1;
			if (rightTop < cell.top)
			{
				drawFace(cell, u, v, true, rightTop);
			}
			if (leftTop < cell.top)
			{
				drawFace(cell, u, v, false, leftTop);
			}

			// Top: the surface block, or the sea over drowned ground; a slight tint
			// by height.
			Rgb top;
			if (cell.top > cell.ground)
			{
				top = Mix({96, 170, 205}, {22, 56, 116}, (cell.top - cell.ground) / 40.0);
			}
			else
			{
				const MatterId topMaterial = materialAt(cell, cell.ground);
				top = colorOf(topMaterial);
				countRock(topMaterial);
			}
			// Hillshade: the light comes from the far corner, so slopes that fall
			// towards the camera are lit and slopes that rise towards it are dark.
			// Slopes are stretched by the vertical exaggeration so gentle ground
			// still shows.
			const auto topAt = [&](int uu, int vv) {
				return at(std::clamp(uu, 0, view.keptU - 1), std::clamp(vv, 0, view.keptV - 1)).top;
			};
			// Measured over about 8 m, not one column: block steps are already drawn
			// as side faces, and a one-column slope would turn every 1 m step into a
			// 45 degree slope and light its edge column up.
			const int reach = std::clamp(static_cast<int>(std::lround(8.0 / step)), 2, 8);
			const double slope =
			    ((topAt(u + reach, v) - topAt(u - reach, v)) + (topAt(u, v + reach) - topAt(u, v - reach))) /
			    (4.0 * reach * step);
			const double lit = std::clamp(1.0 - 2.0 * view.exaggeration * slope, 0.7, 1.3);
			const double tint = (0.92 + 0.16 * (cell.top - lowestGround) / heightSpan) * lit;
			image.FillQuad({view.Project(u, v, cell.top + 1), view.Project(u + 1, v, cell.top + 1),
			                view.Project(u + 1, v + 1, cell.top + 1), view.Project(u, v + 1, cell.top + 1)},
			               Shade(top, tint));
		}
	}

	if (!WritePng(options.output, image))
	{
		std::fprintf(stderr, "Could not write %s.\n", options.output.c_str());
		return 1;
	}

	std::printf("%s: %d x %d px\n", options.output.c_str(), width, height);
	std::printf("place %.4f, %.4f  seed %llu  view %s\n", options.latitude, options.longitude,
	            static_cast<unsigned long long>(options.seed), options.view.c_str());
	std::printf("window %.0f m (%d x %d columns of %.2f m), kept %d x %d\n", extent, columns, columns, step, view.keptU,
	            view.keptV);
	std::printf("centre ground %d m above sea level; sea level at %+d from it\n", terrain.OriginElevationMeters(), sea);
	std::printf("surface %d..%d m around the centre (relief %d m), %.0f%% under water\n", lowestGround, highestTop,
	            highestTop - lowestGround, 100.0 * drowned / static_cast<double>(cells.size()));
	std::printf("picture: %.3f px/m, vertical x%.1f, ground shown %.0f m below the "
	            "lowest surface\n",
	            view.scale, view.exaggeration, depth);
	PrintRocks(registry.Catalog(), rocksSeen);

	PrintPlanetCell(options, world.Map());
	return 0;
}
} // namespace SnapshotTool
