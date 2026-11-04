#include "domains/natural/planetary/geology/PlanetEpochs.h"

#include <algorithm>
#include <iterator>

namespace Infinitesia {
namespace {
constexpr double kEarthAgeMyr = 4540.0;

// ICS International Chronostratigraphic Chart (ages in Ma, rounded to 1 Myr;
// the scale is only meaningful to that resolution on a game planet).
constexpr Epoch kEarthEpochs[] = {
    {"Hadean", EpochRank::Eon, 4540.0, 4000.0},
    {"Archean", EpochRank::Eon, 4000.0, 2500.0},
    {"Proterozoic", EpochRank::Eon, 2500.0, 539.0},
    {"Phanerozoic", EpochRank::Eon, 539.0, 0.0},

    {"Eoarchean", EpochRank::Era, 4000.0, 3600.0},
    {"Paleoarchean", EpochRank::Era, 3600.0, 3200.0},
    {"Mesoarchean", EpochRank::Era, 3200.0, 2800.0},
    {"Neoarchean", EpochRank::Era, 2800.0, 2500.0},
    {"Paleoproterozoic", EpochRank::Era, 2500.0, 1600.0},
    {"Mesoproterozoic", EpochRank::Era, 1600.0, 1000.0},
    {"Neoproterozoic", EpochRank::Era, 1000.0, 539.0},
    {"Paleozoic", EpochRank::Era, 539.0, 252.0},
    {"Mesozoic", EpochRank::Era, 252.0, 66.0},
    {"Cenozoic", EpochRank::Era, 66.0, 0.0},

    {"Siderian", EpochRank::Period, 2500.0, 2300.0},
    {"Rhyacian", EpochRank::Period, 2300.0, 2050.0},
    {"Orosirian", EpochRank::Period, 2050.0, 1800.0},
    {"Statherian", EpochRank::Period, 1800.0, 1600.0},
    {"Calymmian", EpochRank::Period, 1600.0, 1400.0},
    {"Ectasian", EpochRank::Period, 1400.0, 1200.0},
    {"Stenian", EpochRank::Period, 1200.0, 1000.0},
    {"Tonian", EpochRank::Period, 1000.0, 720.0},
    {"Cryogenian", EpochRank::Period, 720.0, 635.0},
    {"Ediacaran", EpochRank::Period, 635.0, 539.0},
    {"Cambrian", EpochRank::Period, 539.0, 485.0},
    {"Ordovician", EpochRank::Period, 485.0, 444.0},
    {"Silurian", EpochRank::Period, 444.0, 419.0},
    {"Devonian", EpochRank::Period, 419.0, 359.0},
    {"Carboniferous", EpochRank::Period, 359.0, 299.0},
    {"Permian", EpochRank::Period, 299.0, 252.0},
    {"Triassic", EpochRank::Period, 252.0, 201.0},
    {"Jurassic", EpochRank::Period, 201.0, 145.0},
    {"Cretaceous", EpochRank::Period, 145.0, 66.0},
    {"Paleogene", EpochRank::Period, 66.0, 23.0},
    {"Neogene", EpochRank::Period, 23.0, 2.6},
    {"Quaternary", EpochRank::Period, 2.6, 0.0},
};

// Milestones of Earth's biosphere (Ma). They are heuristics for gating content,
// not a model of the biosphere (provenance: gameplay).
constexpr double kFreeOxygenFromMyr = 2400.0;  // Great Oxidation Event
constexpr double kBifStartMyr = 3000.0;        // banded iron formations
constexpr double kBifEndMyr = 1800.0;
constexpr double kLandPlantsFromMyr =
    450.0;  // Late Ordovician spores, Silurian vascular plants
constexpr double kOrganicRichMarineFromMyr = 800.0;  // algal black shales
constexpr double kCalcareousPlanktonFromMyr =
    220.0;  // Late Triassic coccolithophores
}  // namespace

PlanetEpochTable::PlanetEpochTable() : PlanetEpochTable(kEarthAgeMyr, true) {}

PlanetEpochTable::PlanetEpochTable(double planetAgeMyr, bool biosphere)
    : m_planetAgeMyr(std::max(planetAgeMyr, 1.0)),
      m_scale(m_planetAgeMyr / kEarthAgeMyr),
      m_biosphere(biosphere) {
  m_epochs.reserve(std::size(kEarthEpochs));
  for (const Epoch& epoch : kEarthEpochs) {
    m_epochs.push_back({epoch.name, epoch.rank, epoch.startMyr * m_scale,
                        epoch.endMyr * m_scale});
  }
}

const Epoch* PlanetEpochTable::EpochAt(double ageMyr,
                                       EpochRank rank) const noexcept {
  if (!(ageMyr >= 0.0) || ageMyr > m_planetAgeMyr) {
    return nullptr;
  }
  // Oldest first; a boundary age belongs to the younger division, so the last
  // match wins.
  const Epoch* found = nullptr;
  for (const Epoch& epoch : m_epochs) {
    if (epoch.rank == rank && ageMyr <= epoch.startMyr &&
        ageMyr >= epoch.endMyr) {
      found = &epoch;
    }
  }
  return found;
}

BiosphereState PlanetEpochTable::BiosphereAt(double ageMyr) const noexcept {
  BiosphereState state;
  if (!m_biosphere || !(ageMyr >= 0.0)) {
    return state;
  }
  const double earthMyr = ToEarthMyr(ageMyr);
  state.freeOxygen = earthMyr <= kFreeOxygenFromMyr;
  state.bandedIronFormations =
      earthMyr <= kBifStartMyr && earthMyr >= kBifEndMyr;
  state.landPlants = earthMyr <= kLandPlantsFromMyr;
  state.organicRichMarineSediments = earthMyr <= kOrganicRichMarineFromMyr;
  state.calcareousPlankton = earthMyr <= kCalcareousPlanktonFromMyr;
  return state;
}
}  // namespace Infinitesia
