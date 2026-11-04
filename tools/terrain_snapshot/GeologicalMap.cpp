#include "terrain_snapshot/GeologicalMap.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <map>
#include <vector>

#include "terrain_snapshot/Image.h"
#include "terrain_snapshot/Reports.h"

namespace SnapshotTool {
using namespace Infinitesia;

int RenderGeologicalMap(const Options& options, const Window& window,
                        const SnapshotWorld& world) {
  const double extent = window.extent;
  const double step = window.step;
  const int columns = window.columns;
  const MatterCatalog& catalog = world.Matter();
  const GeologyModel& geology = world.Geology();
  const SettlementTerrain& terrain = world.Terrain();

  const int pixels = std::max(
      1, static_cast<int>(std::lround(options.scale > 0.0
                                          ? options.scale
                                          : std::max(1.0, 1200.0 / columns))));
  std::vector<int> ground(static_cast<std::size_t>(columns) * columns);
  std::vector<MatterId> rockAt(ground.size(), kUnspecifiedMatter);
  std::vector<char> drowned(ground.size(), 0);
  std::vector<char> faultTrace(
      ground.size(),
      0);  // 1 normal, 2 thrust, 3 strike-slip: the plane meets the ground
  for (int south = 0; south < columns; ++south) {
    for (int east = 0; east < columns; ++east) {
      const int eastMeters =
          static_cast<int>(std::lround(-extent * 0.5 + (east + 0.5) * step));
      const int northMeters =
          static_cast<int>(std::lround(extent * 0.5 - (south + 0.5) * step));
      const SurfaceColumn column = terrain.Column(eastMeters, northMeters);
      const auto profile = geology.ProfileAt(
          column.province, options.originEast + eastMeters + 0.5,
          options.originNorth + northMeters + 0.5, column.pileTopMeters);
      const std::size_t index =
          static_cast<std::size_t>(south) * columns + east;
      ground[index] = column.heightBlocks;
      drowned[index] = column.underSea ? 1 : 0;
      // Under the topsoil (3 m) the rock that outcrops or lies just below the
      // ground.
      const int rockLevel =
          terrain.OriginElevationMeters() + column.heightBlocks - 4;
      rockAt[index] = GeologyModel::RockAt(profile, rockLevel);

      // Where a fault plane reaches the ground the fault shows as a line on the
      // map.
      for (const GeologyModel::Fault& fault : geology.FaultsNear(
               column.province, options.originEast + eastMeters + 0.5,
               options.originNorth + northMeters + 0.5)) {
        const auto effect = GeologyModel::EffectOf(
            fault, options.originEast + eastMeters + 0.5,
            options.originNorth + northMeters + 0.5, column.pileTopMeters);
        const double reach = std::max(
            1.0,
            std::tan(fault.dipDegrees * 3.14159265358979 / 180.0) * step * 0.7);
        if (effect.amplitude > 0.0 &&
            std::abs(rockLevel - effect.cutElevation) <= reach) {
          faultTrace[index] =
              static_cast<char>(1 + static_cast<int>(fault.kind));
        }
      }
    }
  }

  Image image(columns * pixels, columns * pixels, {24, 28, 34});
  std::map<MatterId, long> mapped;
  const auto groundAt = [&](int east, int south) {
    return ground[static_cast<std::size_t>(std::clamp(south, 0, columns - 1)) *
                      columns +
                  std::clamp(east, 0, columns - 1)];
  };
  const int reach = std::clamp(static_cast<int>(std::lround(8.0 / step)), 1, 8);
  for (int south = 0; south < columns; ++south) {
    for (int east = 0; east < columns; ++east) {
      const std::size_t index =
          static_cast<std::size_t>(south) * columns + east;
      const RgbColor rock = catalog.ColorOf(rockAt[index]);
      Rgb color{rock.r, rock.g, rock.b};
      ++mapped[rockAt[index]];

      // Contacts: a dark line where the neighbouring column is another rock.
      const bool contact =
          (east + 1 < columns && rockAt[index + 1] != rockAt[index]) ||
          (south + 1 < columns &&
           rockAt[index + static_cast<std::size_t>(columns)] != rockAt[index]);
      // Hill shading from the north-west, so relief and rock bands read
      // together.
      const double slopeEast =
          (groundAt(east + reach, south) - groundAt(east - reach, south)) /
          (2.0 * reach * step);
      const double slopeNorth =
          (groundAt(east, south - reach) - groundAt(east, south + reach)) /
          (2.0 * reach * step);
      const double lit =
          std::clamp(1.0 + 1.5 * (slopeEast - slopeNorth) * 0.7, 0.75, 1.25);
      color = Shade(color, lit * (contact ? 0.55 : 1.0));
      if (drowned[index] != 0) {
        color = Mix(color, {52, 110, 170}, 0.7);
      }
      if (faultTrace[index] != 0) {
        static const Rgb kFaultColors[] = {
            {220, 40, 40},
            {40, 90, 230},
            {235, 200, 30}};  // normal, thrust, strike-slip
        color = Mix(color, kFaultColors[faultTrace[index] - 1], 0.9);
      }
      const double x0 = static_cast<double>(east) * pixels;
      const double y0 = static_cast<double>(south) * pixels;
      image.FillQuad({Point{x0, y0}, Point{x0 + pixels, y0},
                      Point{x0 + pixels, y0 + pixels}, Point{x0, y0 + pixels}},
                     color);
    }
  }
  if (!WritePng(options.output, image)) {
    std::fprintf(stderr, "Could not write %s.\n", options.output.c_str());
    return 1;
  }
  std::printf(
      "%s: geological map %d x %d px, north up, %.0f m window (%.1f m per "
      "column)\n",
      options.output.c_str(), image.Width(), image.Height(), extent, step);
  PrintRocks(catalog, mapped);
  return 0;
}
}  // namespace SnapshotTool
