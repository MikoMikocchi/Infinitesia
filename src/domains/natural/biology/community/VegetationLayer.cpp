#include "domains/natural/biology/community/VegetationLayer.h"

#include <algorithm>
#include <cmath>

#include "domains/natural/biology/community/Canopy.h"
#include "domains/natural/biology/niche/Suitability.h"
#include "domains/natural/planetary/PlanetLoadSteps.h"
#include "core/PlanetaryCoordinates.h"

namespace Infinitesia {
namespace {
// Version of the stage: raise it whenever its output changes
// (docs/05_PLANET_PIPELINE.md).
constexpr int kStageVersion = 1;

Conditions ConditionsOf(const PlanetRegion& cell) {
  Conditions conditions;
  conditions.temperatureCelsius = cell.temperatureCelsius;
  conditions.precipitationMm = cell.precipitationMm;
  conditions.elevationMeters =
      std::max(0.0, cell.elevationNormalized * kMaxElevationKm * 1000.0);
  conditions.drySeasonMonths = cell.drySeasonMonths;
  conditions.soilFertility = cell.soilFertility;
  conditions.soilDepth = cell.soilDepthNorm;
  return conditions;  // slope is local: the flora looks at it per column
}

VegetationCell GrowCell(const PlanetRegion& cell,
                        const SpeciesCatalog& catalog) {
  VegetationCell result;
  if (cell.isOcean || cell.elevationNormalized <= 0.0) return result;

  const Conditions conditions = ConditionsOf(cell);
  const std::vector<Species>& kinds = catalog.All();

  // The best few kinds of the place, sharpened so that a poor fit gives way to
  // a good one.
  struct Candidate {
    std::uint16_t index;
    double weight;
  };
  std::vector<Candidate> candidates;
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    const double suitability = Suitability(kinds[i].niche, conditions);
    if (suitability > 0.01)
      candidates.push_back(
          {static_cast<std::uint16_t>(i), suitability * suitability});
  }
  if (candidates.empty()) return result;
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) {
                     return a.weight > b.weight;
                   });
  if (candidates.size() > static_cast<std::size_t>(kVegetationSpeciesPerCell))
    candidates.resize(kVegetationSpeciesPerCell);

  double total = 0.0;
  for (const Candidate& candidate : candidates) total += candidate.weight;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    result.species[i] = candidates[i].index;
    result.weight[i] = static_cast<float>(candidates[i].weight / total);
  }
  result.canopyCover = static_cast<float>(CanopyCover(conditions));
  return result;
}

void RunVegetation(PlanetBuild& build, const SpeciesCatalog& catalog) {
  const int latitudeBands = build.LatBands();
  const int longitudeSegments = build.LonSegs();
  Field<VegetationCell>& layer = build.data.layers.Create<VegetationCell>(
      kVegetationLayer, latitudeBands, longitudeSegments);
  for (int la = 0; la < latitudeBands; ++la) {
    for (int lo = 0; lo < longitudeSegments; ++lo)
      layer.At(la, lo) = GrowCell(build.Cell(la, lo), catalog);
    build.ReportRow(la, latitudeBands);
  }
}

int WrapIndex(int index, int count) noexcept {
  return ((index % count) + count) % count;
}
}  // namespace

void AddVegetationStages(PlanetPipeline& pipeline,
                         std::shared_ptr<const SpeciesCatalog> species) {
  pipeline.Add({"vegetation",
                kStageVersion,
                {"terrain", "climate", "soils"},
                {kVegetationLayer},
                LoadStep::kVegetation,
                "Growing vegetation",
                [species = std::move(species)](PlanetBuild& build) {
                  RunVegetation(build, *species);
                }});
}

std::optional<VegetationSample> SampleVegetation(const PlanetMap& map,
                                                 CartesianPoint3D direction) {
  const Field<VegetationCell>* layer =
      map.Layers().Find<VegetationCell>(kVegetationLayer);
  if (layer == nullptr) return std::nullopt;

  const GeographicCoordinates geo =
      PlanetaryCoordinates::ToGeographic(direction);
  const int latitudeBands = layer->LatitudeBands();
  const int longitudeSegments = layer->LongitudeSegments();
  // Cell coordinates with cell centres at integer positions, as the terrain
  // samples the planet.
  const double latitudeCell =
      (geo.latitudeDegrees + 90.0) / 180.0 * latitudeBands - 0.5;
  const double longitudeCell =
      geo.longitudeDegrees / 360.0 * longitudeSegments - 0.5;
  const double latitudeFloor = std::floor(latitudeCell);
  const double longitudeFloor = std::floor(longitudeCell);
  const double tLatitude = latitudeCell - latitudeFloor;
  const double tLongitude = longitudeCell - longitudeFloor;
  const int rows[2] = {
      std::clamp(static_cast<int>(latitudeFloor), 0, latitudeBands - 1),
      std::clamp(static_cast<int>(latitudeFloor) + 1, 0, latitudeBands - 1)};
  const int columns[2] = {
      WrapIndex(static_cast<int>(longitudeFloor), longitudeSegments),
      WrapIndex(static_cast<int>(longitudeFloor) + 1, longitudeSegments)};

  VegetationSample sample;
  double speciesWeight = 0.0;
  for (int row = 0; row < 2; ++row) {
    for (int column = 0; column < 2; ++column) {
      const double share = (row == 0 ? 1.0 - tLatitude : tLatitude) *
                           (column == 0 ? 1.0 - tLongitude : tLongitude);
      if (share <= 0.0) continue;
      const VegetationCell& cell = layer->At(rows[row], columns[column]);
      sample.canopyCover += share * cell.canopyCover;
      if (cell.canopyCover <= 0.0f)
        continue;  // a treeless cell lends no kinds, only its absence of canopy
      for (int i = 0;
           i < kVegetationSpeciesPerCell && cell.species[i] != kNoSpecies;
           ++i) {
        const double weight = share * cell.canopyCover * cell.weight[i];
        auto* found = std::find_if(sample.entries.begin(),
                                   sample.entries.begin() + sample.count,
                                   [&](const VegetationSample::Entry& entry) {
                                     return entry.species == cell.species[i];
                                   });
        if (found == sample.entries.begin() + sample.count) {
          found = &sample.entries[static_cast<std::size_t>(sample.count++)];
          found->species = cell.species[i];
        }
        found->weight += weight;
        speciesWeight += weight;
      }
    }
  }
  for (int i = 0; i < sample.count; ++i)
    sample.entries[static_cast<std::size_t>(i)].weight /= speciesWeight;
  return sample;
}
}  // namespace Infinitesia
