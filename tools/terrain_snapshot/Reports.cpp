#include "terrain_snapshot/Reports.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "domains/natural/planetary/geology/PlanetEpochs.h"
#include "domains/natural/planetary/geology/Province.h"

namespace SnapshotTool {
using namespace Infinitesia;

// The rocks that were drawn, most visible first, with their colours.
void PrintRocks(const MaterialCatalog& catalog,
                const std::map<MaterialId, long>& seen) {
  if (seen.empty()) {
    return;
  }
  std::vector<std::pair<MaterialId, long>> ranked(seen.begin(), seen.end());
  std::sort(ranked.begin(), ranked.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
  long total = 0;
  for (const auto& entry : ranked) {
    total += entry.second;
  }
  std::printf("rocks in view:\n");
  for (const auto& [material, count] : ranked) {
    const MaterialDef* info = catalog.Get(material);
    const RgbColor color = catalog.ColorOf(material);
    std::printf(
        "  #%02x%02x%02x  %-26s %-22s %4.1f%%\n", color.r, color.g, color.b,
        info != nullptr ? info->key.c_str() : "?",
        info != nullptr ? info->name.c_str() : "?",
        100.0 * static_cast<double>(count) / static_cast<double>(total));
  }
}

void PrintPlanetCell(const Options& options, const PlanetMap& map) {
  const int bands = map.GetLatitudeBandCount();
  const int segments = map.GetLongitudeSegmentCount();
  const double longitude =
      options.longitude - 360.0 * std::floor(options.longitude / 360.0);
  const int row =
      std::clamp(static_cast<int>((options.latitude + 90.0) / 180.0 * bands), 0,
                 bands - 1);
  const int column = std::clamp(static_cast<int>(longitude / 360.0 * segments),
                                0, segments - 1);
  const PlanetRegion& cell = map.GetRegion(row, column);
  const Province* province = map.FindProvince(cell);
  if (province != nullptr) {
    std::printf(
        "province %s #%u (%.0f%% land, %d cells); main event %.0f Myr ago, "
        "erosion %s (%.1f km)\n",
        std::string(ProvinceTypeName(province->type)).c_str(), province->id,
        100.0 * province->landFraction, province->cellCount,
        province->eventAgeMyr,
        std::string(ErosionLevelName(province->erosionLevel)).c_str(),
        province->erosionDepthKm);
    const PlanetEpochTable& epochs = map.GetEpochs();
    const Epoch* era = epochs.EpochAt(province->eventAgeMyr, EpochRank::Era);
    const Epoch* period =
        epochs.EpochAt(province->eventAgeMyr, EpochRank::Period);
    std::printf("main event in the %s era%s%s\n",
                era != nullptr ? std::string(era->name).c_str() : "?",
                period != nullptr ? ", " : "",
                period != nullptr ? std::string(period->name).c_str() : "");
  }
  std::printf(
      "crust %.0f Myr old, sediment %.2f km, heat flow %.0f mW/m2, hotspot "
      "%.2f; plate %.1f cm/yr\n",
      cell.crustAgeMyr, cell.sedimentThicknessKm, cell.heatFlowMwPerM2,
      cell.hotspotInfluence,
      std::hypot(cell.plateVelocityEastCmPerYear,
                 cell.plateVelocityNorthCmPerYear));
  if (cell.boundarySubtype != PlateBoundaryKind::None) {
    static constexpr const char* kKinds[] = {"none",
                                             "mid-ocean ridge",
                                             "continental rift",
                                             "oceanic subduction",
                                             "continental subduction",
                                             "continental collision",
                                             "transform fault"};
    std::printf(
        "nearest plate boundary: %s, %.0f km away, relative speed %.1f cm/yr\n",
        kKinds[static_cast<int>(cell.boundarySubtype)],
        cell.distanceToBoundaryKm,
        std::hypot(cell.boundaryRelativeVelocityEastCmPerYear,
                   cell.boundaryRelativeVelocityNorthCmPerYear));
  }
}
}  // namespace SnapshotTool
