#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

#include "domains/natural/geology/model/GeologyModel.h"
#include "domains/natural/geology/model/GeologyModelDetail.h"
#include "domains/natural/geology/util/StableRandom.h"

namespace Infinitesia {
namespace {
using GeologyModelDetail::kDegrees;
using GeologyModelDetail::Lerp;

constexpr std::uint64_t kPileStage = 0x7069'6c65'0001ull;       // "pile"
constexpr std::uint64_t kStructureStage = 0x7374'7275'0002ull;  // "stru"
constexpr std::uint64_t kLayerStage = 0x6c61'7972'0003ull;      // "layr"

// Fold flanks never get steeper than this (the first version has no overturned
// folds).
constexpr double kMaxFoldSlope = 1.7320508075688772;  // tan 60 degrees
constexpr double kFoldWavelengthRatio = 0.45;

// A share of the formations varies so much sideways that it thins to nothing in
// places and survives as lenses (thickness * (1 + variability * noise) <= 0).
constexpr double kLensShare = 0.2;
constexpr double kLensMinVariability = 1.2;
constexpr double kLensMaxVariability = 2.0;

bool Satisfied(BiosphereRequirement requirement, const BiosphereState& state) {
  switch (requirement) {
    case BiosphereRequirement::None:
      return true;
    case BiosphereRequirement::FreeOxygen:
      return state.freeOxygen;
    case BiosphereRequirement::BandedIronFormations:
      return state.bandedIronFormations;
    case BiosphereRequirement::LandPlants:
      return state.landPlants;
    case BiosphereRequirement::OrganicRichMarineSediments:
      return state.organicRichMarineSediments;
    case BiosphereRequirement::CalcareousPlankton:
      return state.calcareousPlankton;
  }
  return true;
}

// How many formations the erosion of the province has stripped off the top.
std::size_t StrippedLayers(ErosionLevel level, std::size_t layerCount) {
  switch (level) {
    case ErosionLevel::Surface:
      return 0;
    case ErosionLevel::Shallow:
      return std::min<std::size_t>(1, layerCount - 1);
    case ErosionLevel::Intermediate:
      return std::min<std::size_t>(2, layerCount - 1);
    case ErosionLevel::Deep:
      return layerCount - 1;
  }
  return 0;
}
}  // namespace

GeologyModel::GeologyModel(std::shared_ptr<const GeologyCatalog> catalog,
                           const GeologyPlanetView& map, GeologyConfig config)
    : m_catalog(std::move(catalog)), m_config(config) {
  if (m_catalog == nullptr || !(config.strataThicknessScale > 0.0)) {
    throw std::invalid_argument(
        "The geology model needs a catalog and a positive strata scale.");
  }
  // The pile a place with no province gets: granite, or the first rock of the
  // catalog when there is no granite; kUnspecifiedMatter when there is no rock
  // at all.
  const MatterDef* granite = m_catalog->Matter().Find("granite");
  if (granite == nullptr) {
    for (const MatterDef& material : m_catalog->Matter().Matter()) {
      if (material.blockClass == BlockClass::Rock) {
        granite = &material;
        break;
      }
    }
  }
  m_fallbackPile = {
      {granite != nullptr ? granite->id : kUnspecifiedMatter, 0}};
  m_fallbackStructure.layers = {LayerShape{}};

  const std::uint64_t seed = map.GetSeed();
  m_piles.reserve(map.GetProvinces().size());
  m_structures.reserve(map.GetProvinces().size());
  for (const Province& province : map.GetProvinces()) {
    const ProvinceColumn& column = m_catalog->ColumnFor(province.type);
    const StructureStyle& style = column.style;

    // The layers the province can have at all: a layer that needs a condition
    // of the biosphere (coal needs land plants) is left out if the time it
    // would have been laid down in comes before the condition. Dropping a layer
    // shifts the times of the others, so this repeats until nothing more goes.
    std::vector<ColumnLayer> bottomUp = column.bottomUp;
    for (int pass = 0; pass < 16; ++pass) {
      const std::vector<TimeSpan> timeline = CoverTimeline(
          map.GetEpochs().PlanetAgeMyr(), province.eventAgeMyr,
          province.meanCrustAgeMyr, province.type, bottomUp.size() - 1);
      bool removed = false;
      for (std::size_t k = 0; k < timeline.size(); ++k) {
        const BiosphereState state = map.GetEpochs().BiosphereAt(
            0.5 * (timeline[k].startMyr + timeline[k].endMyr));
        if (!Satisfied(bottomUp[k + 1].needs, state)) {
          bottomUp.erase(bottomUp.begin() + static_cast<std::ptrdiff_t>(k + 1));
          removed = true;
          break;
        }
      }
      if (!removed) {
        break;
      }
    }
    // Top-down.
    const std::vector<ColumnLayer> layers(bottomUp.rbegin(), bottomUp.rend());

    Structure structure;
    std::vector<Layer> pile;
    const std::size_t stripped =
        StrippedLayers(province.erosionLevel, layers.size());
    for (std::size_t i = stripped; i < layers.size(); ++i) {
      Layer layer;
      layer.rock = layers[i].rock;
      LayerShape shape;
      if (i + 1 < layers.size()) {
        // Formations vary in thickness from place to place; each province gets
        // its own.
        const double jitter =
            0.8 + 0.45 * GeologyRandom::Unit(GeologyRandom::Stream(
                             seed, kPileStage, province.id, i));
        layer.thicknessMeters = std::max(
            1, static_cast<int>(std::lround(layers[i].thicknessMeters *
                                            m_config.strataThicknessScale *
                                            jitter)));
        shape.variability =
            Lerp(style.minVariability, style.maxVariability,
                 GeologyRandom::Unit(GeologyRandom::Stream(
                     seed, kLayerStage, province.id, i * 3 + 0)));
        if (GeologyRandom::Unit(GeologyRandom::Stream(
                seed, kLayerStage, province.id, i * 3 + 100)) < kLensShare) {
          shape.variability =
              Lerp(kLensMinVariability, kLensMaxVariability,
                   GeologyRandom::Unit(GeologyRandom::Stream(
                       seed, kLayerStage, province.id, i * 3 + 200)));
        }
        shape.noiseWavelengthMeters =
            300.0 + 900.0 * GeologyRandom::Unit(GeologyRandom::Stream(
                                seed, kLayerStage, province.id, i * 3 + 1));
        shape.noiseSeed =
            GeologyRandom::Stream(seed, kLayerStage, province.id, i * 3 + 2);
      }
      pile.push_back(layer);
      structure.layers.push_back(shape);
    }

    // Dip and folds. Strongly deformed provinces are compressed along the axis
    // of the plate motion, so their layers fall along it and folds run across
    // it; the others lie in an arbitrary direction.
    const auto draw = [&](std::uint64_t index) {
      return GeologyRandom::Unit(
          GeologyRandom::Stream(seed, kStructureStage, province.id, index));
    };
    structure.seed =
        GeologyRandom::Stream(seed, kStructureStage, province.id, 100);
    structure.dipDegrees =
        Lerp(style.minDipDegrees, style.maxDipDegrees, draw(1));
    const bool compressed = style.maxDipDegrees >= 15.0;
    const double axisAzimuth =
        compressed ? province.compressionAzimuthDegrees : 180.0 * draw(2);
    structure.dipAzimuthDegrees = axisAzimuth + (draw(3) < 0.5 ? 0.0 : 180.0);
    const double slope = std::tan(structure.dipDegrees * kDegrees);
    const double dipAzimuth = structure.dipAzimuthDegrees * kDegrees;
    structure.slopeEast = -slope * std::sin(dipAzimuth);
    structure.slopeNorth = -slope * std::cos(dipAzimuth);

    const double longestWavelength = 1500.0 + 3500.0 * draw(4);
    for (int i = 0; i < style.foldCount; ++i) {
      Fold fold;
      fold.wavelengthMeters =
          longestWavelength * std::pow(kFoldWavelengthRatio, i);
      fold.amplitudeMeters = std::min(
          style.foldAmplitude * std::pow(fold.wavelengthMeters, 0.8),
          kMaxFoldSlope * fold.wavelengthMeters / (2.0 * std::numbers::pi));
      const double direction =
          (axisAzimuth +
           30.0 * (draw(10 + static_cast<std::uint64_t>(i)) - 0.5)) *
          kDegrees;
      fold.directionEast = std::sin(direction);
      fold.directionNorth = std::cos(direction);
      fold.phase =
          2.0 * std::numbers::pi * draw(20 + static_cast<std::uint64_t>(i));
      structure.folds.push_back(fold);
    }
    structure.warpWavelengthMeters = 400.0 + 600.0 * draw(5);
    structure.faults = style.faults;
    structure.compressionAzimuthDegrees = province.compressionAzimuthDegrees;

    // The history behind the pile: the formations of the whole column, bottom
    // first, with the ones erosion stripped off marked.
    HistoryInput input;
    input.seed = seed;
    input.province = province.id;
    input.type = province.type;
    input.planetAgeMyr = map.GetEpochs().PlanetAgeMyr();
    input.eventAgeMyr = province.eventAgeMyr;
    input.crustAgeMyr = province.meanCrustAgeMyr;
    input.dipDegrees = structure.dipDegrees;
    input.foldCount = style.foldCount;
    input.maxFaultSlipMeters = style.faults.maxSlipMeters;
    input.erosionDepthKm = province.erosionDepthKm;
    for (std::size_t b = 0; b < layers.size(); ++b) {
      const std::size_t topDown = layers.size() - 1 - b;
      HistoryFormation formation;
      formation.rock = layers[topDown].rock;
      const ModalComposition* info =
          m_catalog->Matter().ModalOf(formation.rock);
      formation.rockClass =
          info != nullptr ? info->rockClass : RockClass::SedimentaryClastic;
      formation.thicknessMeters = layers[topDown].thicknessMeters;
      formation.erased = topDown < stripped;
      input.formationsBottomUp.push_back(formation);
    }
    ProvinceHistory history = BuildProvinceHistory(input);
    std::vector<int> pileEvents;
    for (std::size_t p = 0; p < pile.size(); ++p) {
      const std::size_t bottomUp = layers.size() - 1 - (stripped + p);
      pileEvents.push_back(history.formationEvents[bottomUp]);
    }
    m_histories.push_back(std::move(history));
    m_pileEvents.push_back(std::move(pileEvents));

    m_piles.push_back(std::move(pile));
    m_structures.push_back(std::move(structure));
  }
}

const std::vector<GeologyModel::Layer>& GeologyModel::PileOf(
    ProvinceId province) const noexcept {
  if (province == kNoProvince || province > m_piles.size()) {
    return m_fallbackPile;
  }
  return m_piles[province - 1];
}

const GeologyModel::Structure& GeologyModel::StructureOf(
    ProvinceId province) const noexcept {
  if (province == kNoProvince || province > m_structures.size()) {
    return m_fallbackStructure;
  }
  return m_structures[province - 1];
}

MatterId GeologyModel::ExposedRock(ProvinceId province) const noexcept {
  return PileOf(province).front().rock;
}
}  // namespace Infinitesia
