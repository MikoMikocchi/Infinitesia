#include <doctest/doctest.h>

#include <cmath>
#include <memory>
#include <set>

#include "domains/natural/biology/community/VegetationLayer.h"
#include "foundation/PlanetaryCoordinates.h"
#include "support/MaterialFixtures.h"
#include "support/Require.h"

using namespace Infinitesia;
using namespace InfinitesiaTests;

namespace {
std::shared_ptr<const SpeciesCatalog> Catalog() {
  static const auto catalog =
      std::make_shared<const SpeciesCatalog>(SpeciesCatalog::Load(
          kAssets / "biology" / "species.json", *SharedMaterials()));
  return catalog;
}

PlanetPipeline VegetationPipeline() {
  PlanetPipeline pipeline = MakeDefaultPlanetPipeline();
  AddVegetationStages(pipeline, Catalog());
  return pipeline;
}

// A coarse planet keeps these tests fast in unoptimized builds.
const PlanetMap& SharedMap() {
  static const PlanetMap map = [] {
    PlanetParameters parameters;
    parameters.latitudeBands = 90;
    parameters.longitudeSegments = 180;
    return PlanetMap(parameters, {}, VegetationPipeline());
  }();
  return map;
}

const Field<VegetationCell>& Layer() {
  return SharedMap().Layers().Require<VegetationCell>(kVegetationLayer);
}
}  // namespace

TEST_CASE("TheVegetationStageRunsAfterWhatItNeedsAndHasAVersion") {
  const PlanetPipeline pipeline = VegetationPipeline();
  std::vector<std::string> names;
  for (const PlanetStage* stage : pipeline.Plan()) names.push_back(stage->name);
  const auto position = [&](const char* name) {
    return std::find(names.begin(), names.end(), name) - names.begin();
  };
  CHECK(position("vegetation") > position("soils"));
  CHECK(position("vegetation") > position("climate.precipitation"));
  CHECK(position("vegetation") > position("terrain"));
  CHECK(pipeline.Fingerprint().find("vegetation:1") != std::string::npos);
}

TEST_CASE("ThePlanetCarriesTheVegetationLayerOnlyWithTheStage") {
  CHECK(SharedMap().Layers().Contains(kVegetationLayer));
  PlanetParameters parameters;
  parameters.latitudeBands = 30;
  parameters.longitudeSegments = 60;
  const PlanetMap plain(parameters);
  CHECK(!plain.Layers().Contains(kVegetationLayer));
  CHECK(!SampleVegetation(plain, {1.0, 0.0, 0.0}).has_value());
}

TEST_CASE("EveryCellOfTheLayerIsConsistent") {
  const PlanetMap& map = SharedMap();
  const Field<VegetationCell>& layer = Layer();
  int forested = 0;
  int land = 0;
  for (int la = 0; la < layer.LatitudeBands(); ++la) {
    for (int lo = 0; lo < layer.LongitudeSegments(); ++lo) {
      const VegetationCell& cell = layer.At(la, lo);
      land += map.GetRegion(la, lo).isOcean ? 0 : 1;
      Require(cell.canopyCover >= 0.0f && cell.canopyCover <= 1.0f,
              "canopy is outside 0..1");
      if (map.GetRegion(la, lo).isOcean)
        Require(cell.canopyCover == 0.0f && cell.species[0] == kNoSpecies,
                "the sea has vegetation");
      if (cell.canopyCover <= 0.0f) continue;
      ++forested;
      float sum = 0.0f;
      bool ended = false;
      for (int i = 0; i < kVegetationSpeciesPerCell; ++i) {
        if (cell.species[i] == kNoSpecies) {
          ended = true;
          continue;
        }
        Require(!ended, "a kind follows an empty slot");
        Require(cell.species[i] < Catalog()->All().size(),
                "a kind is not in the catalog");
        Require(i == 0 || cell.weight[i] <= cell.weight[i - 1],
                "kinds are not sorted by weight");
        sum += cell.weight[i];
      }
      Require(std::abs(sum - 1.0f) < 1e-4f,
              "the weights of a cell do not add up to 1");
    }
  }
  // Some of the land is forest; the rest is ice, desert and the like.
  CHECK(forested > land / 10);
  CHECK(forested < land);
}

TEST_CASE("TheLayerFollowsTheClimateOfThePlanet") {
  const PlanetMap& map = SharedMap();
  const Field<VegetationCell>& layer = Layer();
  std::set<std::string> kinds;
  double warmWetCover = 0.0;
  double coldCover = 0.0;
  int warmWet = 0;
  int cold = 0;
  for (int la = 0; la < layer.LatitudeBands(); ++la) {
    for (int lo = 0; lo < layer.LongitudeSegments(); ++lo) {
      const PlanetRegion& region = map.GetRegion(la, lo);
      const VegetationCell& cell = layer.At(la, lo);
      if (region.isOcean) continue;
      if (cell.species[0] != kNoSpecies)
        kinds.insert(Catalog()->All()[cell.species[0]].key);
      if (region.temperatureCelsius > 5.0 && region.temperatureCelsius < 20.0 &&
          region.precipitationMm > 900.0) {
        warmWetCover += cell.canopyCover;
        ++warmWet;
      }
      if (region.temperatureCelsius < -15.0) {
        coldCover += cell.canopyCover;
        ++cold;
      }
    }
  }
  REQUIRE(warmWet > 0);
  CHECK(warmWetCover / warmWet > 0.4);
  if (cold > 0) CHECK(coldCover / cold < 0.1);
  CHECK(kinds.size() >= 2);
}

TEST_CASE("AGivenSeedGrowsTheSameLayer") {
  PlanetParameters parameters;
  parameters.latitudeBands = 45;
  parameters.longitudeSegments = 90;
  const PlanetMap first(parameters, {}, VegetationPipeline());
  const PlanetMap second(parameters, {}, VegetationPipeline());
  const auto& a = first.Layers().Require<VegetationCell>(kVegetationLayer);
  const auto& b = second.Layers().Require<VegetationCell>(kVegetationLayer);
  for (int la = 0; la < a.LatitudeBands(); ++la) {
    for (int lo = 0; lo < a.LongitudeSegments(); ++lo)
      Require(a.At(la, lo).canopyCover == b.At(la, lo).canopyCover &&
                  a.At(la, lo).species == b.At(la, lo).species,
              "two builds of one planet differ");
  }
}

TEST_CASE("TheSampleAtACellCentreIsTheCell") {
  const PlanetMap& map = SharedMap();
  const Field<VegetationCell>& layer = Layer();
  int checked = 0;
  for (int la = 5; la < layer.LatitudeBands() - 5 && checked < 200; la += 3) {
    for (int lo = 0; lo < layer.LongitudeSegments() && checked < 200; lo += 7) {
      const VegetationCell& cell = layer.At(la, lo);
      if (cell.canopyCover <= 0.0f) continue;
      const auto sample =
          SampleVegetation(map, map.GetPlanet().GetSurfacePoint(la, lo));
      RequireFatal(sample.has_value(), "the layer is missing");
      Require(std::abs(sample->canopyCover - cell.canopyCover) < 0.02,
              "the canopy at a centre is not the cell's");
      Require(sample->count >= 1 && sample->entries[0].species != kNoSpecies,
              "no kinds at the centre of a forest");
      ++checked;
    }
  }
  CHECK(checked > 20);
}

TEST_CASE("TheSampleBlendsNeighbouringCellsWithoutJumps") {
  const PlanetMap& map = SharedMap();
  const Field<VegetationCell>& layer = Layer();
  // Walk along a parallel: between two adjacent cells the canopy changes by
  // steps much smaller than the difference of the cells, and every sample's
  // weights add up to 1.
  const int la = layer.LatitudeBands() / 2 + 8;
  double largestStep = 0.0;
  double largestCellDifference = 0.0;
  std::optional<double> previous;
  for (int step = 0; step < layer.LongitudeSegments() * 8; ++step) {
    const double longitude = 360.0 * step / (layer.LongitudeSegments() * 8);
    const double latitude = map.GetRegion(la, 0).latitudeDegrees;
    const auto sample = SampleVegetation(
        map, PlanetaryCoordinates::ToCartesian({latitude, longitude}));
    RequireFatal(sample.has_value(), "the layer is missing");
    double sum = 0.0;
    for (int i = 0; i < sample->count; ++i)
      sum += sample->entries[static_cast<std::size_t>(i)].weight;
    Require(sample->count == 0 || std::abs(sum - 1.0) < 1e-9,
            "the weights of a sample do not add up to 1");
    if (previous)
      largestStep =
          std::max(largestStep, std::abs(sample->canopyCover - *previous));
    previous = sample->canopyCover;
  }
  for (int lo = 0; lo + 1 < layer.LongitudeSegments(); ++lo)
    largestCellDifference = std::max(
        largestCellDifference,
        static_cast<double>(std::abs(layer.At(la, lo).canopyCover -
                                     layer.At(la, lo + 1).canopyCover)));
  CHECK(largestStep <= largestCellDifference / 4.0 + 1e-6);
}
